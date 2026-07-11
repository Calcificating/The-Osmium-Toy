@echo off
REM usage: build.bat [demo|detcheck|bench|stats|acid|portal|capstone|tests|all]
REM run from Parallelcore/

if "%1"=="" goto :usage
if "%1"=="demo" goto :demo
if "%1"=="detcheck" goto :detcheck
if "%1"=="bench" goto :bench
if "%1"=="stats" goto :stats
if "%1"=="acid" goto :acid
if "%1"=="portal" goto :portal
if "%1"=="capstone" goto :capstone
if "%1"=="tests" goto :tests
if "%1"=="all" goto :all
goto :usage

:demo
echo [demo] building...
g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp portal_index.cpp main.cpp -o demo.exe
echo [demo] done
goto :eof

:detcheck
echo [detcheck] building...
g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp portal_index.cpp detcheck.cpp -o detcheck.exe
echo [detcheck] done
goto :eof

:bench
echo [bench] building...
g++ -std=c++17 -O2 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp portal_index.cpp bench.cpp -o bench.exe
echo [bench] done
goto :eof

:stats
echo [stats] building...
g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp portal_index.cpp statsdemo.cpp -o statsdemo.exe
echo [stats] done
goto :eof

:acid
echo [acid] building...
g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp portal_index.cpp acid_demo.cpp -o acid_demo.exe
echo [acid] done
goto :eof

:portal
echo [portal] building...
g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp portal_index.cpp portal_demo.cpp -o portal_demo.exe
echo [portal] done
goto :eof

:capstone
echo [capstone] building demo...
g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp portal_index.cpp capstone_demo.cpp -o capstone_demo.exe
echo [capstone] building detcheck...
g++ -std=c++17 -O2 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp portal_index.cpp capstone_detcheck.cpp -o capstone_detcheck.exe
echo [capstone] done
goto :eof

:tests
echo [tests] building...
g++ -std=c++17 -O1 -g -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp portal_index.cpp tests\test_runner.cpp -o tests\test_runner.exe
echo [tests] done
goto :eof

:all
echo [all] building all...
call :demo
call :detcheck
call :bench
call :stats
call :acid
call :portal
call :capstone
call :tests
echo [all] done
goto :eof

:usage
echo usage: build.bat [demo^|detcheck^|bench^|stats^|acid^|portal^|capstone^|tests^|all]
goto :eof