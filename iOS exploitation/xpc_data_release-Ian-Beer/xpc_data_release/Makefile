all:
	clang -dynamiclib -o shared_data.dylib shared_data.c -arch i386 -arch x86_64
	clang -o nsxpc_client nsxpc_client.m -lobjc -framework Foundation -framework CoreFoundation 
