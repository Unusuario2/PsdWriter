# PSDWriter

**PSDWriter** is a command-line tool that converts any common image format into `.psd` files for easy editing in Photoshop and seamless compiling with `vtex` in Source engine projects. It uses [`stb`](https://github.com/nothings/stb) for broad image format support and outputs Photoshop-compatible PSD files using [`psd_sdk`](https://github.com/MolecularMatters/psd_sdk).  It also automatically generates `vtex` configuration files using a built-in template for faster material setup.

<img width="1278" height="403" alt="image" src="https://github.com/user-attachments/assets/cb138956-68b0-411c-aa61-fa74fe97b63d" />

---

## 📦 Installation

To install **PSDWriter**, follow these steps:

1. Start with a clean copy of the **Source SDK 2013** source code (works both for SP & MP).
2. Drag and drop the contents of this repository into your SDK source directory.
3. Overwrite any existing files when prompted.
4. Regenerate the Visual Studio solution files (e.g., by running `createallprojects.bat`).
5. Open the solution in Visual Studio and compile it.  
   The `psdwriter` executable will be built and ready to use.

---

## 📄 License

This repository contains various pieces of original source code licensed under the **MIT License**, except for bundled third-party libraries such as [`stb`](https://github.com/nothings/stb) and [`psd_sdk`](https://github.com/MolecularMatters/psd_sdk), which retain their own licenses.

---

## Contact

Steam: https://steamcommunity.com/profiles/76561199073832016/  
Twitter: https://x.com/47Z14  
Discord: carlossuarez7285  

---
