# Wii U WUPS build environment
FROM ghcr.io/wiiu-env/devkitppc:20260225
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20240505 /artifacts $DEVKITPRO
WORKDIR /project
