# Third-party dependencies

These are vendored source snapshots, not Git submodules. The application builds
without downloading packages or running Python. Upstream license files are kept
beside each library. No project-wide license is imposed on the team's own code.

| Directory | Upstream source | Revision | License |
| --- | --- | --- | --- |
| `glfw/` | https://github.com/glfw/glfw | tag `3.5.1`, commit `70a9bb3881fe80fd483236e2b203cb451c6ecf40` | zlib, `glfw/LICENSE.md` |
| `glm/` | https://github.com/g-truc/glm | tag `1.0.3`, commit `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed` | MIT or Modified MIT, `glm/copying.txt` |
| `glad/` | https://github.com/Dav1dde/glad | generator `2.0.8`, tag commit `73db193f853e2ee079bf3ca8a64aa2eaf6459043` | see `glad/LICENSE` and generated file headers |
| `stb/` | https://github.com/nothings/stb | commit `a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d`, stb_image 2.30 | MIT or public domain, `stb/LICENSE` |

GLFW includes its build files, headers, sources and platform dependencies; upstream
examples, tests and documentation are omitted. GLM includes its headers and build
files; upstream tests and documentation are omitted. stb includes `stb_image.h`
and its license only. Library source files are otherwise unmodified.

GLAD is pre-generated for OpenGL 3.3 Core, without extensions, using the generator's
packaged Khronos specifications (`--reproducible`). It loads functions through GLFW.
To regenerate it with Python in a separate development environment:

```sh
python -m pip install glad2==2.0.8
python -m glad --api gl:core=3.3 --extensions "" --out-path third_party/glad --reproducible c
```

Keep `glad/LICENSE` when replacing the generated files. Python and glad2 are
needed only for regeneration, not for building or playing the project.
