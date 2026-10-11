# Textured GX2 renderer setup

This directory contains the GLSL source pair for the Recent Apps textured overlay:

- `RecentAppsTexture.vert` transforms the overlay quad and passes texture coordinates.
- `RecentAppsTexture.frag` samples the RGBA title-icon texture.

## Compile the shader pair

Use the Wii U CafeGLSL compiler from a Wii U development environment:

```sh
glslcompiler -vs src/shaders/RecentAppsTexture.vert -ps src/shaders/RecentAppsTexture.frag -o RecentAppsTexture.gsh
```

Check the compiler output for the shader mode and use that mode when setting up GX2. The vertex and pixel shaders must be compiled as a pair so their shader modes agree.

## Runtime integration requirements

The eventual renderer must load the compiled GFD shader group, initialize the fetch shader and attribute streams, create a GX2 RGBA texture from the decoded 128x128 icon pixels, invalidate the CPU-written texture cache, and bind the texture and sampler before drawing the quad.

Do not draw from the title-icon inspection or notification callbacks. Rendering must run at a verified point in the GX2 frame, preserve/restore the host title's relevant GX2 state, and remain disabled until the hook is verified safe. The current plugin hook only observes the GX2 pipeline; these shader sources do not enable drawing by themselves.

The compiler is an external build dependency; it is intentionally not added to the plugin link flags here, so the existing plugin build does not start depending on CafeGLSL merely to compile the shader source files.
