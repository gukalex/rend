# this is expected to be run under windows' git bash thing, IDK

# download imgui
git clone -b docking --depth 1 https://github.com/ocornut/imgui
# download stb image
curl https://raw.githubusercontent.com/nothings/stb/master/stb_image.h --output stb_image.h
# download httplib
curl https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h --output httplib.h
# download GLFW
curl -JL https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.bin.WIN64.zip --output glfw.zip
unzip glfw.zip
mkdir -p lib
# copy GLFW files into appropriate folders. IMPORTANT: use appropriate VS version (lib-vc20...)
cp glfw-3.3.8.bin.WIN64/lib-vc2022/glfw3_mt.lib lib/glfw3_mt.lib
cp -r glfw-3.3.8.bin.WIN64/include/GLFW GLFW

# download VERY important files
curl https://clipart-library.com/images_k/angry-pepe-transparent/angry-pepe-transparent-11.png --output pepe.png
curl https://i.redd.it/xprpkp063sn51.png --output amogus.png
curl https://www.pngmart.com/files/22/Crying-Cat-Meme-PNG-Pic.png --output pool.png
curl https://i.pinimg.com/originals/ce/1d/46/ce1d4643187f4d3f208e9c2ef84e4f6e.jpg --output din.jpg
curl https://image.similarpng.com/very-thumbnail/2021/08/Coffee-logo-illustration-on-transparent-background-PNG.png --output coffee.png
curl https://www.pngall.com/wp-content/uploads/3/Portal-PNG.png --output portal1.png