// input: framebuffer dimensions; output: static lock screen; pos: desktop sleep presentation.
#pragma once
static void lock_screen(unsigned *sequence) {
    int w=display_width(), h=display_height();
    unsigned char *pixels=malloc((size_t)w*h);
    if(!pixels)return;
    memset(pixels,255,(size_t)w*h);
    int cx=w/2, cy=h/2;
    // An outlined padlock remains legible with the frontlight off. No timer or animation.
    for(int y=cy-115;y<cy+95;y++)for(int x=cx-90;x<cx+90;x++){
        int body=y>=cy-15 && (x<cx-78 || x>=cx+78 || y<cy-3 || y>=cy+83);
        int shackle=y<cy-15 && x>=cx-58 && x<cx+58 &&
            (x<cx-46 || x>=cx+46 || y<cy-103);
        int keyhole=x>=cx-7 && x<cx+7 && y>=cy+23 && y<cy+58;
        if(body||shackle||keyhole)pixels[y*w+x]=0;
    }
    display_update(++*sequence,0,0,w,h,DISPLAY_CLEAN,pixels);
    free(pixels);
}
