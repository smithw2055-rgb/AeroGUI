# Third-party source dependencies

These dependencies are Git submodules pinned by the parent repository:

| Dependency | Upstream | Version | Commit |
| --- | --- | --- | --- |
| FreeType | https://github.com/freetype/freetype | `VER-2-13-3` | `42608f77f20749dd6ddc9e0536788eaad70ea4b5` |
| HarfBuzz | https://github.com/harfbuzz/harfbuzz | `14.2.1` | `56feae4035bdd48f62ba2b8d8c16232d4d89b3a4` |
| Expat | https://github.com/libexpat/libexpat | `R_2_8_1` | `c7ffbf3879f6aef7a7b020ef84ddb4ee00222b19` |

Initialize a fresh checkout with:

```sh
git submodule update --init --recursive
```

The upstream license files remain in each submodule.
