Particle Simulation with DirectX 11 and ImGui
This repository contains a particle simulation application built using DirectX 11 for rendering and compute shaders, and Dear ImGui for the user interface. The project demonstrates advanced graphics programming concepts, including particle systems, compute shaders, and camera manipulation.
---
Features
Core Features
•	Particle Simulation:
•	Particles are initialized in a 3D grid and updated using compute shaders.
•	Gravity, floor collision, and velocity are simulated in real-time.
•	Adjustable grid size and simulation parameters via the UI.
•	Camera System:
•	Free-fly camera with yaw, pitch, and position controls.
•	Keyboard and mouse input for movement and rotation.
•	Rendering:
•	3D rendering of particles using vertex and pixel shaders.
•	Support for quads and cubes as particle representations.
User Interface
•	Dear ImGui Integration:
•	Real-time control of simulation parameters (e.g., gravity, floor height, grid size).
•	Toggleable control window for user interaction.
Additional Features
•	DirectX 11 Integration:
•	Swap chain resizing for dynamic window resizing.
•	Depth and rasterizer state management.
•	Compute Shaders:
•	GPU-based particle updates for high performance.
•	ImGui Backend:
•	Custom Win32 and DirectX 11 backend for seamless UI integration.
---
Prerequisites
Software Requirements
•	Windows 10/11 (or compatible version).
•	Visual Studio 2022 with the following workloads:
•	Desktop development with C++.
•	Game development with C++ (for DirectX support).
Libraries and Dependencies
•	DirectX 11: Core graphics API for rendering and compute shaders.
•	Dear ImGui: For the graphical user interface.
•	stb_image_write: For optional image export functionality.
---
Getting Started
Clone the Repository
