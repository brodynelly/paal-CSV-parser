# Contributing

## Getting Started

1. Fork and clone the repo
2. Create a branch: `git checkout -b feat/your-feature`
3. Make changes and verify the build compiles cleanly
4. Commit with [Conventional Commits](https://www.conventionalcommits.org/): `feat: add thing`
5. Push and open a pull request

## Commit Format

```
<type>: <short description>

Types: feat, fix, docs, chore, refactor, test, ci, build, perf
```

## Code Style

Run `clang-format -i **/*.{cpp,h}` before committing.
Follow the project's `.clang-format` config.

## Build Requirements

- C++17 compatible compiler (GCC 7+ or Clang 5+)
- CMake 3.10+
- MongoDB C++ driver (`libmongocxx`)

## Issues

Open an issue before starting large changes. Bug fixes and doc updates can go straight to a PR.
