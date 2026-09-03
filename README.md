# diffmap
Calculating difference maps of electron microscopy reconstructions.

Credit goes to Grigorieff Lab!
You can find the original source code at:  
https://grigoriefflab.umassmed.edu/diffmap

This version compiles on Linux (x86_64, aarch64) and MacOS (arm64), and works for very large volumes (1280 pixel cubes tested).
Changes were made with the help of AI.

I use the executable through a bash script that takes 3 inputs:
```
diffmap map1.mrc map2.mrc pixelsize
```

The output is a difference map: **diff.mrc** and **map2_scaled_to_map1**

to compile, execute the following command in the source directory:

```
make -f Makefile
```

By default, after a preparatory step, the program will use all available processors.
To use a specific number of threads use:

```
OMP_NUM_THREADS=2 ./diffmap.exe ...
```
