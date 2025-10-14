# Terrainer

**Terrainer** is a Godot 4 plugin for generating and editing terrain.
It provides GPU-based, high-performance terrain rendering using heightmaps, capable of displaying large and detailed worlds efficiently.

---

## 🌄 Overview

Terrainer renders the entire terrain using a single **MultiMesh**, meaning all chunks share the same mesh regardless of the level of detail (LOD). This approach minimizes draw calls and maintains performance even with vast landscapes.

The LOD transitions are handled smoothly in the vertex shader, following the principles of the [CDLOD](https://github.com/fstrugar/CDLOD) technique. This allows seamless blending between detail levels without visible popping.

---

## 🚀 Features

- [x] Automatic adjustment of LOD levels
- [x] GPU vertex morphing for smooth LOD transitions
- [ ] Heightmap-based terrain shape definition
- [ ] Streaming system for chunk data (load only what’s needed)
- [ ] In-editor terrain editing tools
- [ ] Biome painting (define shape and textures per biome)

---

## 🧩 Compatibility

- **Engine:** Godot 4.5
  *(Earlier versions haven’t been tested yet)*

---

## 🛠️ Getting Started

You can use Terrainer as either a **GDExtension** or a **Godot module**:

- [GDExtension docs](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/index.html)
- [Module integration guide](https://docs.godotengine.org/en/stable/engine_details/architecture/custom_modules_in_cpp.html)

A ready-to-go **[demo project](https://github.com/TopScales/terrainer_demo)** is available with **Windows binaries** if you just want to test things out.

### Installation

The **preferred way** to use this plugin is by getting the [demo project](https://github.com/TopScales/terrainer_demo). There, you will find instruction on how to generate your own binaries in case you need them.

An alternative approach is to compile a custom Godot editor that includes Terrainer as a module. To do this, follow the next steps:

1. Make a directory for Godot to look for external modules, e.g. `<my-modules>`.
2. Go inside the modules directory and clone this repo.

```shell
cd <my-modules>
git clone https://github.com/TopScales/terrainer_demo
```

3. [Compile Godot](https://docs.godotengine.org/en/stable/engine_details/development/compiling/index.html) as usual providing the `custom_modules` option.

```shell
scons custom_modules=<my-modules>
```

---

## 🤝 Contributing

Contributions, feedback, and ideas are always welcome!
If you’d like to help out:

1. Fork the repo
2. Create a new branch (`feature/amazing-thing`)
3. Commit your changes
4. Open a pull request 🎉

---

## 🧾 License

This addon has been released under the [MIT License](https://github.com/TopScales/terrainer/blob/master/LICENSE)

---

## 🙌 Acknowledgements

- Inspired by the [CDLOD](https://github.com/fstrugar/CDLOD) terrain rendering technique by **Filip Strugar**.
- Built with ❤️ for the **Godot Engine** community.

