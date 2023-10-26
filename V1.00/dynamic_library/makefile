TARGET = csapp
# 头文件路径
HEADPATH = ../include/
CXX = gcc
CXXFLAGS = -c -fPIC

all:
	$(CXX) $(CXXFLAGS) $(TARGET).c -o $(TARGET).o -I $(HEADPATH)
	$(CXX) -shared $(TARGET).o -o lib$(TARGET).so
	rm $(TARGET).o
clean:
	rm *.so
