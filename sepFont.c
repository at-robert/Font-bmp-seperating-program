#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef unsigned long U32;

static int FONT_WIDTH=0;
static int FONT_HEIGHT=0;

#define b4_to_u32(b0, b1, b2, b3)       ((b0) + ((U32)(b1)<<8) + ((U32)(b2)<<16) + ((U32)(b3)<<24))
#define u32_to_u8(ul, pos)				(((ul)>>((pos)*8))&0xFF)
#define GET_LINE_SIZE(ww, dd)	(((ww)*(dd)/8+3)/4*4)

int check_vline_empty(FILE *fp, int offset, int width, int height, int depth, int line)
{
    unsigned long tmp;
    unsigned char data[4];
    int i;
    
    fseek(fp, offset+(line*depth/8), SEEK_SET);
    for(i=0;i<height;i++)
    {
         memset(data, 0, 4);
         if(depth/8==0)
         {
             fread(data, 1, 1, fp);
             tmp = data[0]>>((line*depth)%8);
         }
         else
         {
             fread(data, 1, depth/8, fp);
             tmp = b4_to_u32(data[0], data[1], data[2], data[3]);
             //printf("%x ", tmp);
         }
         if(tmp!=(unsigned)((1<<depth)-1))
         {
             return 0;
         }
         fseek(fp, GET_LINE_SIZE(width, depth)-depth/8, SEEK_CUR);
    }
    return 1;
    
}

int dump_vline_until_empty(FILE *fp, int offset, int width, int height, int depth, int line, unsigned char *buf)
{
    unsigned long tmp[32*32];
    unsigned char data[4];
    int i, j, cnt=0, lineCnt=0, k;
    int flag;

    do {
        fseek(fp, offset+((line+lineCnt)*depth/8), SEEK_SET);
        flag = 1;
        for(i=0;i<height;i++)
        {
             memset(data, 0, 4);
             if(depth/8==0)
             {
                 fread(data, 1, 1, fp);
                 tmp[cnt] = data[0]>>((line*depth)%8);
             }
             else
             {
                 fread(data, 1, depth/8, fp);
                 tmp[cnt] = b4_to_u32(data[0], data[1], data[2], data[3]);
                 //printf("%x ", tmp);
             }
             
             if(tmp[cnt]!=(unsigned)((1<<depth)-1))
             {
                 flag = 0;
             }
             cnt++;
             fseek(fp, GET_LINE_SIZE(width, depth)-depth/8, SEEK_CUR);
        }
        lineCnt++;
    }while(flag==0);

     cnt = 0;
     for(i=0;i<height;i++)
     {
         for(j=0;j<lineCnt;j++)
         {
             if(depth/8>0)
             {
                 for(k=0;k<depth/8;k++)
                     buf[cnt++] = u32_to_u8(tmp[j*height+i], k);
             }
             else
             {
                 // TODO:
             }
         }
         k = (lineCnt*depth/8)%4;
         for(;k%4!=0;k++)
             buf[cnt++] = 0;         
     }
    
    return lineCnt;
}

int barcode(int argc, char *argv[])
{
    FILE *fpSrc, *fpDest; 
    char *fileSrc;
    char fileDest[100];
    unsigned char header[256], *hptr=header, bb;
    unsigned long offset, depth, width, height;
    int i, j;
    char ch;
    unsigned char flag=0;
    int count = 0;
    unsigned char bmpraw[32*32*4];
    char prefix[32] = "";

    opterr = 0;
    while((ch=getopt(argc, argv, "w:h:f:p:"))!=-1)
    {
        switch(ch)
        {
        case 'w':
                sscanf(optarg, "%d", &FONT_WIDTH);
                flag |= 0x01;
                break;
        case 'h':
                sscanf(optarg, "%d", &FONT_HEIGHT);
                flag |= 0x02;
                break;
        case 'f':
                fileSrc = strdup(optarg);
                flag |= 0x04;
                break;
        case 'p':
                strcpy(prefix, optarg);
                break;
        default:
                fprintf(stderr, "invalid argument - \"%c\"!!\n", ch);
                return -1;
        }
    }
    
    if((flag&0x07) != 0x07)
    {
        fprintf(stderr, "Usage: barcode -f FILE -w WIDTH -h HEIGHT -p prefix\n");
        return -1;
    }
    
    printf("FONT = (%d, %d)\n", FONT_WIDTH, FONT_HEIGHT); 
    printf("fileSrc = %s\n", fileSrc);
    
    fpSrc = fopen(fileSrc, "rb");

    fread(hptr, 1, 2, fpSrc); // header
    hptr+=2;
    fread(hptr, 1, 4, fpSrc); // size
    hptr+=4;
    fread(hptr, 1, 4, fpSrc); // reserved
    hptr+=4;
    fread(hptr, 1, 4, fpSrc); // offset
    offset = b4_to_u32(hptr[0], hptr[1], hptr[2], hptr[3]);
    printf("offset = %x\n", offset);
    hptr+=4;    
    fread(hptr, 1, 4, fpSrc); // header size
    hptr+=4;
    fread(hptr, 1, 4, fpSrc); // width
    width = b4_to_u32(hptr[0], hptr[1], hptr[2], hptr[3]);
    if(width!=FONT_WIDTH)
    {
        printf("bmp width (%d) shall meet the argument %d !!\n", width, FONT_WIDTH);
        fclose(fpSrc);
        return -1;
    }
    hptr+=4;
    fread(hptr, 1, 4, fpSrc); // height
    height = b4_to_u32(hptr[0], hptr[1], hptr[2], hptr[3]);

    hptr[0] = u32_to_u8(FONT_HEIGHT, 0);
    hptr[1] = u32_to_u8(FONT_HEIGHT, 1);
    hptr[2] = u32_to_u8(FONT_HEIGHT, 2);
    hptr[3] = u32_to_u8(FONT_HEIGHT, 3);

    hptr+=4;
    fread(hptr, 1, 2, fpSrc); // planes
    hptr+=2;
    fread(hptr, 1, 2, fpSrc); // bits per pixel
    depth = b4_to_u32(hptr[0], hptr[1], 0, 0);
    hptr+=2;

    printf("w x h x bpp = (%d, %d, %d)\n", width, height, depth);
    fread(hptr, 1, offset-(int)(hptr-header), fpSrc);

    if(strlen(prefix)==0)
        strcpy(prefix, "font_");
        
    j = 0;
    while(!feof(fpSrc))
    {
        j=fread(bmpraw, 1, GET_LINE_SIZE(FONT_WIDTH, depth)*FONT_HEIGHT, fpSrc);
        printf("j = %d\n", j);
        if(j>0)
        {
            sprintf(fileDest, "%s_%02d.bmp", prefix, count++);
            if(j!=GET_LINE_SIZE(FONT_WIDTH, depth)*FONT_HEIGHT)
            {
                header[22] = u32_to_u8(j/GET_LINE_SIZE(FONT_WIDTH, depth), 0);
                header[23] = u32_to_u8(j/GET_LINE_SIZE(FONT_WIDTH, depth), 1);
                header[24] = u32_to_u8(j/GET_LINE_SIZE(FONT_WIDTH, depth), 2);
                header[25] = u32_to_u8(j/GET_LINE_SIZE(FONT_WIDTH, depth), 3);
            }
            else
            {
                header[22] = u32_to_u8(FONT_HEIGHT, 0);
                header[23] = u32_to_u8(FONT_HEIGHT, 1);
                header[24] = u32_to_u8(FONT_HEIGHT, 2);
                header[25] = u32_to_u8(FONT_HEIGHT, 3);
            }
                
            fpDest = fopen(fileDest, "wb");
            fwrite(header, 1,  offset, fpDest);
            fwrite(bmpraw, 1, j, fpDest);
            j = offset%4;
            bb=0;
            for(;j%4!=0;j++)
                fwrite(&bb, 1, 1, fpDest);            
            fclose(fpDest);
        }
    }
    fclose(fpSrc);
    
}


int sepFont(int argc, char *argv[])
{
    FILE *fpSrc, *fpDest; 
    char *fileSrc;
    char fileDest[100];
    unsigned char header[256], *hptr=header, bb;
    unsigned long offset, depth, width, height;
    int i, j, k;
    char ch;
    unsigned char flag=0;
    int count = 0;
    unsigned char bmpraw[32*32*4];
    char prefix[32] = "";

    opterr = 0;
    while((ch=getopt(argc, argv, "w:h:f:c:p:"))!=-1)
    {
            switch(ch)
            {
            case 'w':
                    sscanf(optarg, "%d", &FONT_WIDTH);
                    flag |= 0x01;
                    break;
            case 'h':
                    sscanf(optarg, "%d", &FONT_HEIGHT);
                    flag |= 0x02;
                    break;
            case 'f':
                    fileSrc = strdup(optarg);
                    flag |= 0x04;
                    break;
            case 'c':
                    sscanf(optarg, "%x", &count);
                    break;
            case 'p':
                    strcpy(prefix, optarg);
                    break;
            default:
                    fprintf(stderr, "invalid argument - \"%c\"!!\n", ch);
                    return -1;
            }
    }
    
    if((flag&0x07) != 0x07)
    {
        fprintf(stderr, "Usage: sepFont -f FILE -w WIDTH -h HEIGHT -c start_code_in_hex -p prefix_of_out_files\n");
        return -1;
    }
    
    printf("FONT = (%d, %d)\n", FONT_WIDTH, FONT_HEIGHT); 
    printf("fileSrc = %s\n", fileSrc);
    
    fpSrc = fopen(fileSrc, "rb");

    fread(hptr, 1, 2, fpSrc); // header
    hptr+=2;
    fread(hptr, 1, 4, fpSrc); // size
    hptr+=4;
    fread(hptr, 1, 4, fpSrc); // reserved
    hptr+=4;
    fread(hptr, 1, 4, fpSrc); // offset
    offset = b4_to_u32(hptr[0], hptr[1], hptr[2], hptr[3]);
    printf("offset = %x\n", offset);
    hptr+=4;    
    fread(hptr, 1, 4, fpSrc); // header size
    hptr+=4;
    fread(hptr, 1, 4, fpSrc); // width
    width = b4_to_u32(hptr[0], hptr[1], hptr[2], hptr[3]);
    if(0)//(width!=FONT_WIDTH)
    {
        printf("bmp width (%d) shall meet the argument %d !!\n", width, FONT_WIDTH);
        fclose(fpSrc);
        return -1;
    }
    hptr+=4;
    fread(hptr, 1, 4, fpSrc); // height
    height = b4_to_u32(hptr[0], hptr[1], hptr[2], hptr[3]);
    if((height%FONT_HEIGHT)!=0)
    {
        printf("bmp height (%d) shall equal to times of the argument %d !!\n", height, FONT_HEIGHT);
        fclose(fpSrc);
        return -1;
    }
#if 0
    hptr[0] = u32_to_u8(FONT_HEIGHT, 0);
    hptr[1] = u32_to_u8(FONT_HEIGHT, 1);
    hptr[2] = u32_to_u8(FONT_HEIGHT, 2);
    hptr[3] = u32_to_u8(FONT_HEIGHT, 3);
#endif
    hptr+=4;
    fread(hptr, 1, 2, fpSrc); // planes
    hptr+=2;
    fread(hptr, 1, 2, fpSrc); // bits per pixel
    depth = b4_to_u32(hptr[0], hptr[1], 0, 0);
    hptr+=2;

    printf("w x h x bpp = (%d, %d, %d)\n", width, height, depth);
    fread(hptr, 1, offset-(int)(hptr-header), fpSrc);

    if(strlen(prefix)==0)
        strcpy(prefix, "font_");
        
    j = 0;
    while(j<width)
    {
        i = dump_vline_until_empty(fpSrc, offset, width, height, depth, j, bmpraw);
        if(i>1)
        {
            sprintf(fileDest, "%s%04X.bmp", prefix, count++);
            fpDest = fopen(fileDest, "wb");
            header[18] = u32_to_u8(i, 0);
            header[19] = u32_to_u8(i, 1);
            header[20] = u32_to_u8(i, 2);
            header[21] = u32_to_u8(i, 3);
            fwrite(header, 1,  offset, fpDest);
            fwrite(bmpraw, 1, height*GET_LINE_SIZE(i, depth), fpDest);
            k = offset%4;
            bb=0;
            for(;k%4!=0;k++)
                fwrite(&bb, 1, 1, fpDest);            
            fclose(fpDest);
        }
        j += i;
    }
    
    //for(i=0;i<width;i++)
    //    printf("%d ", check_vline_empty(fpSrc, offset, width, height, depth, i));
    //printf("\n");
    
#if 0
    sprintf(fileDest, "font_%02d.bmp", 0);
    fpDest = fopen(fileDest, "wb");
    fwrite(header, 1,  offset, fpDest);

    for(i=0;i<height;i++)
    {
        if(i%FONT_HEIGHT==0)
        {
            sprintf(fileDest, "font_%02d.bmp", i/FONT_HEIGHT+count);
            fpDest = fopen(fileDest, "wb");
            fwrite(header, 1,  offset, fpDest);
        }
        for(j=0;j<(width*depth/8+3)/4*4;j++)
        {
            fread(&bb, 1, 1, fpSrc);
            fwrite(&bb, 1, 1, fpDest);
        }
        if(i%FONT_HEIGHT==FONT_HEIGHT-1)
        {
            j = offset%4;
            bb=0;
            for(;j%4!=0;j++)
                fwrite(&bb, 1, 1, fpDest);
            fclose(fpDest);
        }
    }
#endif
    fclose(fpSrc);
    
}
