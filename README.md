# CT Scan Visualizer

Visualize CT scans as point clouds. Currently only supports a stack of TIFF
images as input.

![screenshot.png](https://github.com/joe-legg/ct-scan-visualizer/blob/master/screenshot.png)

Note: I made this project as an exercise to learn the basics of OpenGL. It
doesn't serve any practical purpose and is unfinished in its current state.
There are still a few improvements and fixes to be made.

Dependencies:

- GLFW
- OpenGL
- LibTIFF
- cglm

Run `make && ./visualizer` to build execute the program.

## Test Data

The test files in the `microtus_oregoni` folder
(uwbm:mammal specimens:OG-1284 ark:/87602/m4/M154135) can be found here:
https://www.morphosource.org/concern/biological_specimens/000S41001.

The test files in the `peromyscus_gossypinus` folder
(uf:mammals:10393 ark:/87602/m4/M140627) can be found here:
https://www.morphosource.org/concern/biological_specimens/000S36726.
