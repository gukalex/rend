#build object files for libraries we are not gonna touch (slow):
if [[ ! -f http.o || $1 = "ALL" ]] # todo: check all 
then
    echo "recompiling obj files"
    g++ -c http.cpp std.cpp -O3 -std=c++17
fi
#build it (consider moving std.cpp/rend.cpp in the line above):
echo "compiling client min"
g++ client_min.cpp std.o http.o -O3 -std=c++17 -lpthread -o client_min
