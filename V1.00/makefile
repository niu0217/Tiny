TARGET = tinyWeb
#指定动态库路径和头文件的路径
LDFLAGS = -L ./dynamic_library/ -I ./include/ 
#指定在生成可执行文件时需要的动态库
LIBS = -lcsapp -lpthread
CXX = gcc

$(TARGET):
	$(CXX) -g -o $(TARGET) tiny.c $(LIBS) $(LDFLAGS)
clean:
	rm $(TARGET)

