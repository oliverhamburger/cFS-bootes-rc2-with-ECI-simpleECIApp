# Core Flight System : Framework : App : Sample

This repository contains a sample application (sample_app), which is a framework component of the Core Flight System.

This sample application is a non-flight example application implementation for the cFS Bundle. It is intended to be located in the `apps/sample_app` subdirectory of a cFS Mission Tree. The Core Flight System is bundled at <https://github.com/nasa/cFS> (which includes sample_app as a submodule), which includes build and execution instructions.

sample_app is an example for how to build and link an application in cFS. See also the skeleton_app (<https://github.com/nasa/skeleton_app>) if you are looking for a bare-bones application to which to add your business logic.

## Version History

### Development Build: 1.1.0+dev65

- Add build number and baseline to version report
- Install unit test as part of cmake recipe. Sample app test runner now shows up in expected install directory
- See <https://github.com/nasa/sample_app/pull/86>

### Development Build: 1.1.11

- Move the table to fsw/tables and renames "sample_table" to "sample_app_table
- See <https://github.com/nasa/sample_app/pull/76>

### Development Build: 1.1.10

- Test cases now compare an expected event string with a string derived from the spec string and arguments that were output by the unit under test.
- Replace references to `ccsds.h` types with the `cfe_sb.h`-provided type. 
- See <https://github.com/nasa/sample_app/pull/71>

### Development Build: 1.1.9

- Applies the CFE_SB_MsgIdToValue() and CFE_SB_ValueToMsgId() routines where compatibility with an integer MsgId is necessary - syslog prints, events, compile-time MID #define values.
- No more format conversion error in RTEMS build
- See <https://github.com/nasa/sample_app/pull/63>

### Development Build: 1.1.8

- Coverage data from make lcov includes the sample_app code
- See <https://github.com/nasa/sample_app/pull/62>

### Development Build: 1.1.7

- Fix bug where table is not released after being used
- Minor updates (see <https://github.com/nasa/sample_app/pull/52>)

### Development Build: 1.1.6

- Minor updates (see <https://github.com/nasa/sample_app/pull/49>)

### Development Build: 1.1.5

- Fix to build on RASPBIAN OS
- Minor updates (see <https://github.com/nasa/sample_app/pull/47>)

### Development Build: 1.1.4

- Fix for a clean build with OMIT_DEPRECATED
- Minor updates (see <https://github.com/nasa/sample_app/pull/44>)

### Development Build: 1.1.3

- Minor updates (see <https://github.com/nasa/sample_app/pull/34>)

### Development Build: 1.1.2

- Minor updates (see <https://github.com/nasa/sample_app/pull/20>)

### Development Build: 1.1.1

- Minor updates (see <https://github.com/nasa/sample_app/pull/15>)

### _**OFFICIAL RELEASE: 1.1.0 - Aquila**_

- Minor updates (see <https://github.com/nasa/sample_app/pull/11>)
- Not backwards compatible with OSAL 4.2.1
- Released as part of cFE 6.7.0, Apache 2.0

### _**OFFICIAL RELEASE: 1.0.0a**_

- Released as part of cFE 6.6.0a, Apache 2.0

## Known issues

As a sample application, extensive testing is not performed prior to release and only minimal functionality is included. Note discrepancies likely exist between this application and the example detailed in the application developer guide.

## Getting Help

For best results, submit issues:questions or issues:help wanted requests at <https://github.com/nasa/cFS>.

Official cFS page: <http://cfs.gsfc.nasa.gov>
