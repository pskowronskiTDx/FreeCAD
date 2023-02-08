## Compiling Freecad

0. This compiline note are tested for Freecad 0.20 with libpack 12.5.4_x64_VC17 on windows 10 with powershell

1. Clone local repository
	`git clone https://github.com/palapalal/FreeCAD.git`
	
	Switch to correct branch
	 `cd FreeCAD && git checkout 3dmouseintegration`

	

2. Download precompiled dependencies
	`wget "https://github.com/apeltauer/FreeCAD/releases/download/LibPack_12.5.4/FreeCADLibs_12.5.4_x64_VC17.7z" -outfile "FreeCADLibs_12.5.4_x64_VC17.7z"`

3. Uncompress libpack 
	`&"C:\Program Files\7-Zip\7z.exe" x .\FreeCADLibs_12.5.4_x64_VC17.7z`
       
4. Compile Freecad

  - `mkdir -p build/debug; cd build/debug;`
  - `set LIBPACK_DIR ..\..\FreeCADLibs_12.5.4_x64_VC17 `
  - `Start-Process cmake -ArgumentList "-DFREECAD_LIBPACK_DIR=${LIBPACK_DIR} -DPYTHON_INCLUDE_DIR=${LIBPACK_DIR}\bin\include -DPYTHON_LIBRARY=${LIBPACK_DIR}\bin\libs\python38.lib  -DPYTHON_DEBUG_LIBRARY=${LIBPACK_DIR}\bin\libs\python38_d.lib -DPYTHON_EXECUTABLE=${LIBPACK_DIR}\bin\python.exe -DFREECAD_COPY_DEPEND_DIRS_TO_BUILD=ON -DFREECAD_COPY_LIBPACK_BIN_TO_BUILD=ON -DFREECAD_COPY_PLUGINS_BIN_TO_BUILD=ON -DCMAKE_INSTALL_PREFIX=${pwd}/install ../.." -NoNewWindow`
  - `cmake --build . -j8` or open visual studio sln

 Note that changes from our master branch can be seen in https://github.com/palapalal/FreeCAD/pull/8/files, then you can see navlib files location etc..
