# SeisComP main repository

This repository contains main SeisComP applications.

The repository cannot be built standalone. It needs to be integrated
into the `seiscomp` build environment and checked out into
`src/base/main`.

```
$ git clone [host]/seiscomp.git
$ cd seiscomp/src/base
$ git clone [host]/main.git
```

# Build

## Configuration

|Option|Default|Description|
|------|-------|-----------|
SC_TRUNK_LOCATOR_ILOC|OFF|Enable iLoc locator plugin (still experimental)|

## Compilation

Follow the build instructions from the `seiscomp` repository.
