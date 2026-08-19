app.exe: src/main.cpp src/graphics/graphics.cpp src/load/load.cpp third_party/glad/src/glad.c
	g++ -Ithird_party/glad/include -Ithird_party/glfw/include -Ithird_party $^ -Lthird_party/glfw/lib -lglfw3 -lopengl32 -lgdi32 -o $@

clean:
	rm -f app.exe
