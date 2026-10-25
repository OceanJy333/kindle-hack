// input: authenticated commands; output: physical battery and frontlight controls; pos: Voyage settings API.
#pragma once
int device_status(int fd);
int device_light(int level,int automatic);
