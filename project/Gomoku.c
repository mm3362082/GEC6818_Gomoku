/*
	基于GEC6818开发板五子棋游戏
*/
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <linux/input.h>

#define SIZE 15
//定义空白，黑子，白子数据
#define WHITE -1
#define BLACK 1
#define BLANK 0
//屏幕文件地址
#define LCD_PATH "/dev/fb0"
//BMP图片文件存储位置
//主界面
#define MENU_PATH "/gomoku/photo/menu.bmp"
//棋盘背景
#define BACK_PATH "/gomoku/photo/background.bmp"

static int lcd_id=-1;//屏幕文件描述符
static int *lcd_mmap=NULL;//屏幕文件映射
int lcd_size;//屏幕文件大小
struct fb_var_screeninfo lcd_info;//屏幕文件属性数据
int outx=-1,outy=-1;//触摸屏无压感坐标
int x=-1,y=-1;//触摸屏触摸坐标
int blackwin=0,whitewin=0;//黑子白字获胜局数
int chessboard[15][15]={0};//棋盘数组

//绘制模块
void lcd_draw(int x,int y,int color);
//屏幕初始化模块
void lcd_Init();
//读取并绘制模块
void bmp_draw(int x,int y,const char *bmppath);
//主菜单界面
void draw_menu();
//执行游戏
void person_person();
//判断是否已满
int is_full(int chessboard[SIZE][SIZE]);
//判断胜负
int is_win(int chessboard[SIZE][SIZE], int row, int col);
//输入子事件模块
void lcd_event();
//退出程序模块
void quit();
//排行榜模块
void options();

int main () 
{
	//屏幕初始化
	lcd_Init();
    //执行主菜单界面
	draw_menu();
	return 0;
}

//绘制模块
void lcd_draw(int x,int y,int color)
{
	if(x>=0 && x<lcd_info.xres && y>=0 && y<lcd_info.yres)
		*(lcd_mmap+x+y*lcd_info.xres)=color;
}

//读取并绘制bmp文件模块
void bmp_draw(int x,int y,const char *bmppath)
{
	//打开bmp文件
	int bmp_id=open(bmppath,O_RDONLY);
	if(bmp_id==-1)
	{
		perror("");
		exit(0);
	}
    // 定义一个4字节的空间去存储读取到的数据

    unsigned char data[4]={0};

    read(bmp_id,data,2);

    // 判断是否为BMP图片
    if(data[0]!= 0x42 || data[1] != 0x4d)
    {
        puts("this picture not bmp file!");
        return;
    }

    // 读取像素数组的偏移量数据
    lseek(bmp_id,0x0a,SEEK_SET);
    read(bmp_id,data,4);
    // 数据还原，取决大小端模式
    int offset = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];

    // 读取图片宽度
    lseek(bmp_id,0x12,SEEK_SET);
    read(bmp_id,data,4);
    // 数据还原，取决大小端模式
    int width = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];

    // 读取图片高度
    read(bmp_id,data,4);
    // 数据还原，取决大小端模式
    int height = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];

    // 读取色深
    lseek(bmp_id,0x1c,SEEK_SET);
    read(bmp_id,data,2);
    // 数据还原，取决大小端模式
    int depth = data[1] << 8 | data[0];

    printf("weight:%d,height:%d,depth:%d\n",width,height,depth);

    // 偏移到像素数组的位置
    lseek(bmp_id,offset,SEEK_SET);

    // 计算填充字节数
    int fills = 0;

    if((width*depth/8) % 4)
    {
        fills = 4 - (width*depth/8) % 4 ;
    }

    // 实际一行的字节数
    int real_width = width*depth/8 + fills;

    // 计算出像素数组的大小
    int pixel_array_size = real_width*abs(height);

    // 获取像素数组的数据
    unsigned char *color_point = (unsigned char*)malloc(pixel_array_size);
	int i=0;
    read(bmp_id,color_point,pixel_array_size);
    // 显示图片
    for(int h = 0;h < abs(height);h++)
    {
        for(int w = 0;w < width;w++)
        {
            unsigned char a,r,g,b;
            b = color_point[i++];
            g = color_point[i++];
            r = color_point[i++];
            a = depth == 24?0:color_point[i++];

            // 整合颜色
            int color = a << 24|r << 16|g << 8|b;
            lcd_draw(w+x,((height < 0)?h:(height-1-h))+y,color);
        }
        // 每一行结束跳过填充字节
        i+=fills;
    }
	free(color_point);
	close(bmp_id);
}

//屏幕初始化模块
void lcd_Init()
{
	lcd_id=open(LCD_PATH,O_RDWR);
	if(lcd_id==-1)
	{
		perror("");
		exit(0);
	}
	//读取屏幕文件属性数据
	ioctl(lcd_id,FBIOGET_VSCREENINFO,&lcd_info);
	//屏幕文件映射
	//计算出屏幕文件大小
	lcd_size=lcd_info.xres * lcd_info.yres * (lcd_info.bits_per_pixel / 8);
	lcd_mmap=mmap(NULL,lcd_size,PROT_READ|PROT_WRITE,MAP_SHARED,lcd_id,0);
	if(lcd_mmap==MAP_FAILED)
	{
		perror("");
		close(lcd_id);
        lcd_id = -1;
		exit(0);
	}
	return;
	
}

//主菜单界面
void draw_menu() 
{
	//绘制主菜单界面
	while(1)
	{
		//绘制主菜单界面
		bmp_draw(0,0,MENU_PATH);
		//获取输入子事件
		lcd_event();
		if(outx>=239 && outx<=560)
		{
			if(outy>=139 && outy<=211)
			{
				//对战界面
				person_person();
			}
			else if(outy>=259 && outy<=313)
			{
				//排行榜界面
				options();
			}
			else if(outy>=361 && outy<=415)
			{
				//退出程序界面
				quit();
			}
		}
	}
}

//获取输入子事件模块
void lcd_event()
{
	//打开输入子事件文件
	FILE *abs_screen = fopen("/dev/input/event0","r");
	if(abs_screen == NULL)
        exit(0);
    // 用来获取事件的
    struct input_event ev;
    // 循环读取
    while(1)
    {
        int size = fread(&ev,sizeof(ev),1,abs_screen);
        if(size == 0)
            continue;
        // 判断事件类型
        if(ev.type == EV_ABS) // 绝对事件
        {
            // 获取X轴的数值
            if(ev.code == ABS_X)
            {
                x = ev.value*0.78;
            }
        }
        if(ev.type == EV_ABS) // 绝对事件
        {
            // 获取X轴的数值
            if(ev.code == ABS_Y)
            {
                y = ev.value*0.78;
            }
        }
        // 判断压感
        if(ev.type == EV_KEY && ev.code == BTN_TOUCH && ev.value == 0)
        {
            outx=x;
			outy=y;
			return;
		}
	}
}

//游戏对战
void person_person() 
{
    
    //棋盘背景绘制
	bmp_draw(0,0,BACK_PATH);
	for(int i=0;i<SIZE;i++)
	{
		for(int j=0;j<SIZE;j++)
		{
			chessboard[i][j]=0;
		}
	}
	for (int step = 1; step <= SIZE * SIZE; step++) //黑子先行，然后双方轮流下棋
    {   
		int COL=-1,ROW=-1; 
		if (step%2==1) 
        {   
			//当前步数为单数，黑棋落子。
			while (1) 
            {
				lcd_event();
				if(outx>=698 && outx<=746 && outy>=142 && outy<=161)
				{
					return;
				}
				COL=outx/37;
				ROW=outy/30;
				if (chessboard[ROW][COL] != 0) 
                {
					printf("this can't play\n");        //棋子只能落在空白处
					continue;
				}
				if (COL >= SIZE || ROW >=SIZE || COL < 0 || ROW < 0) 
                {
					printf("please play again\n");      //棋子坐标不可超出棋盘
					continue;
				}
				break;
			}
        	//修改数组数据
			chessboard[ROW][COL] = BLACK;
        	//绘制新的棋盘
			for(int y=0;y<lcd_info.yres;y++)
			{
				for(int x=0;x<lcd_info.xres;x++)
				{
					if( ( (y- (23+ROW*30) )*( y- (23+ROW*30) )+( x-(28+COL*37) )*( x-(28+COL*37) ) ) <= 100 )
					{
						lcd_draw(x,y,0x00000000);
					}
				}
			}
            //判断胜负
			if (is_win(chessboard,SIZE,SIZE) == BLACK) 
            {
				printf("blackwin\n");
				blackwin++;
				bmp_draw(300,180,"/gomoku/photo/black.bmp");
				sleep(3);
				return;
			}
		} 
        else if (step % 2 == 0) 
        {                            
			
			while (1) 
            {
				lcd_event();
				if(outx>=698 && outx<=746 && outy>=142 && outy<=161)
				{
					return;
				}
				COL=outx/37;
				ROW=outy/30;
				
				if (chessboard[ROW][COL] != 0) 
                {
					printf("this can't play\n");        //棋子只能落在空白处
					continue;
				}
				if (COL >= SIZE || ROW >= SIZE || COL < 0 || ROW < 0) {
					printf("please play again\n");     //棋子坐标不可超出棋盘
					continue;
				}
				break;
			}
            //修改数组数据
			chessboard[ROW][COL] = WHITE;
            //绘制新的棋盘
			for(int y=0;y<lcd_info.yres;y++)
			{
				for(int x=0;x<lcd_info.xres;x++)
				{
					if( ( (y- (23+ROW*30) )*( y- (23+ROW*30) )+( x-(28+COL*37) )*( x-(28+COL*37) ) ) <= 100 )
					{
						lcd_draw(x,y,0x00ffffff);
					}
				}
			}
            //判断胜负
			if (is_win(chessboard,SIZE,SIZE) == WHITE) 
            {
				printf("whitewin\n");
				whitewin++;
				bmp_draw(300,180,"/gomoku/photo/white.bmp");
				sleep(3);
				return;
			}
		}
		//判断棋盘是否已满
		if (is_full(chessboard) == 1)
		{
			printf("full\n");
			bmp_draw(300,180,"/gomoku/photo/peace.bmp");
			sleep(3);
			return;
		}	
	}
}

//判断棋盘是否已满
int is_full(int chessboard[SIZE][SIZE]) 
{
	int ret = 1;
	for (int i = 0; i < SIZE; i++) 
    {
		for (int j = 0; j < SIZE; j++) 
        {
			if (chessboard[i][j] == BLANK) 
            {//遍历数组，当有一个位置为空，则棋盘不满
				ret = 0;
				break;
			}
		}
	}
	return ret;
}

//判断胜负
int is_win(int chessboard[SIZE][SIZE], int row, int col) 
{
	int i, j;
	for (i = 0; i < row; i++) 
    {
		for (j = 0; j < col; j++) 
        {
			if (chessboard[i][j] == BLANK)
				continue;
			if (j < col - 4)
				if (chessboard[i][j] == chessboard[i][j + 1] && chessboard[i][j] == chessboard[i][j + 2]
				        && chessboard[i][j] == chessboard[i][j + 3] && chessboard[i][j] == chessboard[i][j + 4])
					return chessboard[i][j];
			if (i < row - 4)
				if (chessboard[i][j] == chessboard[i + 1][j] && chessboard[i][j] == chessboard[i + 2][j]
				        && chessboard[i][j] == chessboard[i + 3][j] && chessboard[i][j] == chessboard[i + 4][j])
					return chessboard[i][j];
			if (i < row - 4 && j < col - 4)
				if (chessboard[i][j] == chessboard[i + 1][j + 1] && chessboard[i][j] == chessboard[i + 2][j + 2]
				        && chessboard[i][j] == chessboard[i + 3][j + 3] && chessboard[i][j] == chessboard[i + 4][j + 4])
					return chessboard[i][j];
			if (i < row - 4 && j > 4)
				if (chessboard[i][j] == chessboard[i + 1][j - 1] && chessboard[i][j] == chessboard[i + 2][j - 2]
				        && chessboard[i][j] == chessboard[i + 3][j - 3] && chessboard[i][j] == chessboard[i + 4][j - 4])
					return chessboard[i][j];
		}
	}
	return BLANK;
}

//退出程序模块
void quit()
{
	bmp_draw(0,0,"/gomoku/photo/quit.bmp");
	sleep(1);
	close(lcd_id);
	lcd_id=-1;
	munmap(lcd_mmap,lcd_size);
	exit(0);
}

//排行榜模块
void options()
{
	printf("black win :%d\nwhite win :%d\n",blackwin,whitewin);
	if(blackwin==0 && whitewin==0)
	{
		bmp_draw(0,0,"/gomoku/photo/nogame.bmp");
		sleep(3);
	}
	else
	{
		if(blackwin>=whitewin)
		{
			bmp_draw(0,0,"/gomoku/photo/blackwin.bmp");
			sleep(3);
		}
		else
		{
			bmp_draw(0,0,"/gomoku/photo/whitewin.bmp");
			sleep(3);
		}
	}
	return;
}