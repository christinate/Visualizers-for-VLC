#pragma once
bool GeissInitialize();
void GeissShutdown();
const unsigned char* GeissRender(const unsigned char wave[2][576],const unsigned char spectrum[2][576],float fps,int* width,int* height);
void GeissKey(int key);
int GeissKeyState();
