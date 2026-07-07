quick notes, not a design doc, just so i dont forget what state this is in

## what this is
spike for the parallel-first rewrite. lives outside src/simulation on purpose,
not touching real Simulation.cpp/Particle.h yet. once the double buffer +
command queue + chunking pattern actually proves out on this toy grid ill
look at porting the pattern into the real Parts/pmap stuff. that repo is
genuinely massive (109kb Simulation.cpp lol) so no way am i rewriting... that... 
before knowing what approach even works.

only 4 elements: sand, water, steam, fire. keeps
the decide logic small enough to reason about while the plumbing is still
changing every day.

## architecture as of now
- World has cur/nxt buffers (world.h/cpp)
- decide phase reads cur only, never writes to it, pushes Cmd's to a
  per-thread queue (elements.cpp)
- world split into 4 horizontal chunks, one thread per chunk does the
  decide phase (sim.cpp)
- after threads join, queues get concat'd into one vector
- resolver runs SINGLE THREADED over the merged list and writes into nxt
  (resolver.cpp)
- swap buffers, repeat

resolver being single threaded is deliberate for now, not a limitation im
worried about yet. wanted to get the read/write separation correct before
even thinking about making resolve parallel too.

## known issues / stuff to fix
- resolver checks w.at() (cur) to see if a MOVE destination is free, but it
  should check w.atNext() since an earlier cmd in the same merged list may
  have already claimed that cell this tick. means two particles can stomp
  each other in the same tick sometimes. havent seen it visually break yet
  but its definitely there, its just the demo scene doesnt cause it much
- decideSand has "if (y + 1 <= HEIGHT)" which should be "< HEIGHT". when
  sand is on the actual last row this reads/writes w.at(x, HEIGHT) which is
  one row past the buffer. didnt crash in the 200 tick demo since the sand
  never made it all the way to row 89, but its a landmine, dont trust this
  near the bottom edge yet
- decideChunk loop is "for (y = startY; y <= endY; y++)" and endY is
  startY + chunkSize, so the row at each chunk boundary probably gets
  processed twice (once as the last row of chunk N, once as first row of
  chunk N+1's range since off by one). doesnt crash, just means boundary
  rows get double the update chance, probably visible as slightly faster
  sand at the seams if you stare at it long enough. havent verified this
  one closely, just noticed the math looks wrong when writing it
- using rand() inside decideSand/Water/Steam, called from multiple threads
  at once (each chunk thread calls into these). rand() isnt actually
  documented as thread safe, it "works" here but should be a thread_local
  rng per chunk instead. exactly the kind of thing this whole project is
  supposed to catch before it becomes a habit lol
- CMD_CREATE doesnt copy temp/life from the source particle, e.g. water
  turning to steam always resets to room temp. fine for now since we dont
  visually check temp, will matter once fire heating actually matters
- NUM_CHUNKS is hardcoded to 4, not hardware_concurrency(). doing this on
  purpose while debugging so chunk boundaries are consistent between runs,
  but need to make it a config/cli arg before this goes anywhere

## stuff that seems to actually work
- double buffer separation feels right, decide phase never mutates cur,
  pretty easy to reason about
- command queue pattern is nice, elements dont know anything about threads
  or chunks, they just emit intents
- chunking + std::thread for phase 1 was way less painful than expected,
  the actual footgun is all in phase 2 (resolver) which is still serial
  anyway so no data races there currently, just the logic bug above

## next steps (unordered)
- fix the resolver dest check (nxt not cur)
- fix sand boundary off by one
- fix chunk range off by one
- try making resolveCommands itself chunk-local + parallel, only fall back
  to a serial pass for conflicts that cross chunk boundaries. this is
  probably the actual interesting part of the whole project
- pull NUM_CHUNKS from hardware_concurrency with a manual override flag
- once resolver's parallel-safe, look at what porting this into the real
  Parts array / pmap in src/simulation would look like. Particle.h already
  has temp/life/tmp fields so the mapping isnt crazy, x/y being floats
  there instead of grid ints is the main thing that'll need thought

## how to run it
g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp main.cpp -o spike
./spike
prints an ascii downsampled grid every 20 ticks, 200 ticks total. not
pretty but enough to eyeball whether sand/water/fire are behaving sanely.
