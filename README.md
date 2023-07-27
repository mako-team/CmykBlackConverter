# CmykBlackConverter

A Mako Core SDK example that makes use of a custom transform to find and replace a rich black with a K-only black.

For the purposes of this example:

* A rich black is defined as a CMYK black with `100%` K & non-zero values for C, M or Y
* a K-only black is one with ink on the K channel only, i.e. `C=0%, M=0%, Y=0%, K=100%`

It operates on PDF only.

Enter `CmykBlackConverter -h` to see the usage:

```plain
Convert rich black to K-only black

Usage:
  CmykBlackConverter [OPTION...] <input file> [<output file>]

  -d, --devicen    Use a DeviceN (spot) colour black, instead of a
                   DeviceCMYK black
  -o, --overprint  Do *not* set overprint on changed objects
  -h, --help       Show this Usage information
```

## Using this code

You will need a Mako NuGet package for C++. Just drop it into the `LocalPackages` folder. In Visual Studio's NuGet Package Manager, select `localpackages` from the Sources menu (gear icon top right) then choose the Mako package from the list. The project is set to use the `MakoCore.OEM.Win-x64.VS2019.Static` package, version 7.0.0.183 (Mako 7 release).
