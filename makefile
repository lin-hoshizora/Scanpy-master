TARGET = scanpy.so

$(TARGET): scanpy.o camera_ctrl.o libempia_xuctrl.so
	g++ -shared -Wl,--export-dynamic $^ -o $@ -lpython3.6m -lboost_python-py36

scanpy.o: x50.cc
	g++ -c -fPIC -std=c++11 -I /usr/include/python3.6 $< -o $@

camera_ctrl.o: camera_ctrl.c
	gcc -c -no-pie -fPIC -o $@ $<

clean:
	rm -f *.o libcamera_ctrl.a $(TARGET)
