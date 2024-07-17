// SO (sound output) terminal emulation

#pragma once

int InitSound(int freq);
void FreeSound(void);
void pop_sample(int l, int r);
