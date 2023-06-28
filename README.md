# Operating Systems Project 2: Emulating the Beta-machine

## Project Description
The beta-machine, as introduced in the Computation Structures course, is a toy RISC-like CPU based computer. It is as simple as possible for pedagogical purposes. While it is a nice tool to teach about CPU architecture, its very basic functionalities and instruction set make it a perfect subject to learn about full CPU emulation. In this project, you will write an emulator for the (extended) beta-machine. In addition, you will implement an interrupt handler in beta-assembly.

## How to Use the Project
### Compilation
```bash
gcc ‘pkg-config --cflags gtk4‘ *.c ‘pkg-config --libs gtk4‘ -lm -Wno-deprecated-declarations
```

### Usage
Use the graphical interface.
