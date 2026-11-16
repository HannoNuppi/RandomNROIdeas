# Build-time image decoder

The build helper downloads `stb_image.h` from `nothings/stb` at the immutable
commit listed in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

The header is cached locally at `third_party/stb/stb_image.h` and ignored by
Git. It is only a compile-time dependency. The built NRO decodes JPEG and PNG
files locally and does not access the network.
