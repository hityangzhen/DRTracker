#include <iostream>
using namespace std;
const int MostLayers = 100;
class CElevator
{
public:
	CElevator();//构造函数
	CElevator(int uLayers, int lLayers);//构造函数
	CElevator(CElevator &myElevator);//拷贝构造函数
	void PressOuterButton(int direction, int floor);//电梯外部各楼层操作按钮
	void PressInnerDigitalButton(int floor);//电梯内部数字按钮
	void PressOpen_CloseButton(int operate);//电梯内部开合控制按钮
	void PressAlert();//电梯内部报警按钮
	void run();//电梯运行
	~CElevator();//析构函数
protected:
	void OpenElevator();//打开电梯
	void CloseElevator();//关闭电梯
	void UserOperate();//由于程序单线程运行，因此将用户的所有操作集中在每一层停止的时候完成。
	const int UpperLayers; //电梯最高能到达的楼层
	const int LowerLayers; //电梯最低能到达的楼层
	int States[5]; //电梯当前状态，States[0]表示运行或停止，0停止，1运行，初值0；States[1]表示当前所在楼层，初值1；States[2]表示运行方向，向上0，向下1，初值0；States[3]表示开合，0开，1合，初值1；States[4]表示是否故障，0正常，1故障，初值1
	int DigitalButton[MostLayers];//整形数组，i为楼层且i!=0，i<0时下标i－LowerLayers表示电梯内地下i楼层按钮是否被按下，i>0，i－LowerLayers-1表示电梯内地上i楼层按钮是否被按下，0未按，1按下
	int OuterDownButton[MostLayers];//整形数组，i为楼层且i!=0，i<0时下标i－LowerLayers表示电梯外i层向下按钮是否被按下，i>0，i－LowerLayers-1表示电梯外i层向下按钮是否被按下，0未按，1按下
	int OuterUpButton[MostLayers];//整形数组，i为楼层且i!=0，i<0时下标i－LowerLayers表示电梯外i层向上按钮是否被按下，i>0，i－LowerLayers-1表示电梯外i层向上按钮是否被按下，0未按，1按下
};



CElevator::CElevator():UpperLayers(10),LowerLayers(1)
{  
	States[0] = 0; //初始状态为停止
	States[1] = 1;//初始楼层在1楼
	States[2] = 0;//初始方向为向上
	States[3] = 1;//初始电梯为合
	States[4] = 0;//初始电梯正常
	for(int i=0;i<MostLayers;i++)
	{
		DigitalButton[i] = 0;
		OuterDownButton[i] = 0;
		OuterUpButton[i] = 0;
	}
}

CElevator::CElevator(int uLayers, int lLayers):UpperLayers(uLayers),LowerLayers(lLayers)
{   
	States[0] = 0; //初始状态为停止
	States[1] = 1;//初始楼层在楼
	States[2] =0;//初始方向为向上
	States[3] = 1;//初始电梯为合
	States[4] = 0;//初始电梯正常
	for(int i=0;i<MostLayers;i++)
	{
		DigitalButton[i] = 0;
		OuterDownButton[i] = 0;
		OuterUpButton[i] = 0;
	}	
}


CElevator::CElevator(CElevator &myElevator):UpperLayers(myElevator.UpperLayers),LowerLayers( myElevator.LowerLayers)
{
	States[0] = myElevator.States[0]; 
	States[1] = myElevator.States[1];
	States[2] = myElevator.States[2];
	States[3] = myElevator.States[3];
	States[4] = myElevator.States[4];

	for(int i=0;i<MostLayers;i++)
	{
		DigitalButton[i] = myElevator.DigitalButton[i];	
		OuterDownButton[i] = myElevator.DigitalButton[i];	
		OuterUpButton[i] = myElevator.DigitalButton[i];
	}
}
CElevator::~CElevator()
{
}
void CElevator::PressOuterButton(int direction, int floor)
{
	if(direction==0)
		OuterUpButton[floor-LowerLayers-1] = 1;
	else
		OuterDownButton[floor-LowerLayers-1] = 1;
}
void CElevator::PressInnerDigitalButton(int floor)
{
	DigitalButton[floor-LowerLayers-1] = 1;
}
void CElevator::PressOpen_CloseButton(int operate)
{
	if(operate == 0)   //打开电梯
	{
		OpenElevator();
	}
	else             //关闭电梯
	{
		CloseElevator();
	}
}
void CElevator::OpenElevator()
{
	if(States[3] == 0)
	{
		cout<<"电梯已打开，请先出后进！"<<endl<<endl;
	}
	else
	{
		cout<<"电梯正在打开… 电梯已打开，请先出后进！"<<endl<<endl;
		States[3] = 0;
	}
}
void CElevator::CloseElevator()
{
	if(States[3] == 1)
	{
		cout<<"电梯已关闭，请保持安静！"<<endl<<endl;
	}
	else
	{
		cout<<"电梯正在关闭… 电梯已关闭，请保持安静！"<<endl<<endl;
		States[3] = 1;
	}
}
void CElevator::PressAlert()
{
	cout<<"电梯内部有人报警，请援助！"<<endl;
	States[4] = 1;
}
void CElevator::UserOperate()
{
	//按报警按钮
	int alm = 0;
	cout<<"按报警按钮吗？输入1表示按，0表示不按:";
	cin>>alm;
	cout<<endl;
	if(alm==1)
	{
		States[4] = 1;
		while(States[4]==1)
		{
			cout<<"电梯出现故障，请维修！"<<endl<<endl;
			cout<<"正在维修.......大家保持冷静"<<endl;
			cout<<endl;
			cout<<endl;
			cout<<"已修好";
			cout<<endl;
			cout<<endl;
			for(int i=0;i<100;i++);
			break;
		}
	}
	OpenElevator();
	//电梯内部输入到达楼层
	int sel;
	cout<<"请输入要上还是要下,0上1下:";
	cin>>sel;
	States[2] =sel;    //初始方向为向上
	cout<<"请电梯内乘客输入所要到达的楼层！多个楼层中间用空格隔开，结束输入0："<<endl;
	int floor = 1;
	while(true)
	{
		cin>>floor;
		if(floor==0)
		{
			break;
		}
		if((floor>=LowerLayers) && (floor<=UpperLayers) && (floor!=States[1]))
		{
			if(floor<0)    //地下
			{
				DigitalButton[floor-LowerLayers] = 1;
			}
			else          //地上
			{
				DigitalButton[floor-LowerLayers-1] = 1;//没有楼
			}
		}
	}
	cout<<endl;
	//电梯外部各楼层输入上楼信号
	cout<<"请电梯外部乘客输入上楼信号！多个楼层用空格隔开，结束输入0："<<endl;
	while(true)
	{
		cin>>floor;
		if(floor==0)
		{
			break;
		}
		else
		{
			if((floor>=LowerLayers) && (floor<UpperLayers) && (floor!=States[1]))
			{
				if(floor<0)   //地下
				{
					OuterUpButton[floor-LowerLayers] = 1;
				}
				else         //地上
				{
					OuterUpButton[floor-LowerLayers-1] = 1;//没有楼
				}
			}
		}
	}
	cout<<endl;
	//电梯外部各楼层输入下楼信号
	cout<<"请电梯外部乘客输入下楼信号,多个楼层用空格隔开，结束输入0:"<<endl;
	while(true)
	{
		cin>>floor;
		if(floor==0)
		{
			break;
		}
		else
		{
			if((floor>LowerLayers) && (floor<=UpperLayers) && (floor!=States[1]))
			{
				if(floor<0)   //地下
				{
					OuterDownButton[floor-LowerLayers] = 1;
				}
				else    //地上
				{
					OuterDownButton[floor-LowerLayers-1] = 1;  //没有楼
				}
			}
		}
	}
	cout<<endl;
	//按开电梯按钮
	int direction = 0;
	cout<<"电梯将要关闭，等人请按打开按钮！输入1表示按，0表示不按："<<endl;
	cin>>direction;
	cout<<endl;
	if(direction==1)
	{
		OpenElevator();
		for(int i=0;i<100;i++);  //延时
		CloseElevator();
	}
	//按关电梯按钮	
	cout<<"按关电梯按钮吗？输入1表示按，0表示不按：";
	cin>>direction;
	cout<<endl;
	if(direction==0)
	{
		for(int i=0;i<100;i++);//延时
		CloseElevator();
	}
	else
	{
		CloseElevator();
	}	
}
void CElevator::run()
{
	while(true)
	{
		if(States[0]==0)
		{
			cout<<"电梯停在"<<States[1]<<"层！"<<endl;
			if(States[2]==0)   //向上
			{
				if(States[1]<0)  //地下
				{
					OuterUpButton[States[1]-LowerLayers] = 0;//将记录电梯States[1]层有向上的标志取消
					DigitalButton[States[1]-LowerLayers] = 0;//将记录电梯在States[1]层停靠的标志取消
				}
				else//地上
				{
					OuterUpButton[States[1]-LowerLayers-1] = 0;//将记录电梯States[1]层有向上的标志取消
					DigitalButton[States[1]-LowerLayers-1] = 0;//将记录电梯在States[1]层停靠的标志取消
				}				
			}
			else//向下
			{
				if(States[1]<0)//地下
				{
					OuterDownButton[States[1]-LowerLayers] = 0;//将记录电梯States[1]层有向下的标志取消
					DigitalButton[States[1]-LowerLayers] = 0;//将记录电梯在States[1]层停靠的标志取消
				}
				else//地上
				{
					OuterDownButton[States[1]-LowerLayers-1] = 0;//将记录电梯States[1]层有向下的标志取消
					DigitalButton[States[1]-LowerLayers-1] = 0;//将记录电梯在States[1]层停靠的标志取消
				}	
			}
			if(States[1]==1)
			{
				cout<<"终止电梯程序运行吗？终止输入1，继续运行输入0:";
				int temp = 0;
				cin>>temp;
				if(temp==1)
				{
					exit(0);
				}
			}
			cout<<endl;
			UserOperate();
			States[0] = 1;
		}
		else
		{
			if(States[2]==0)    //向上
			{
				States[1]++;
				if(States[1]==0)
				{
					States[1]++;  //没有层
				}else
				{
					cout<<"电梯向上运行，将要到达"<<States[1]<<"层！"<<endl<<endl;
					if(States[1]==UpperLayers)   //向上到最顶，必定要停，且方向变为向下
					{							
						States[2] = 1;  //变方向
						States[0] = 0;
					}
					else
					{
						if(OuterUpButton[States[1]-LowerLayers-1]==0 && DigitalButton[States[1]-LowerLayers-1]==0)
						{
							cout<<"没有乘客在"<<States[1]<<"层上下，电梯继续向上运行!"<<endl<<endl;							
						}
						else
						{
							States[0] = 0;
						}
					}					
				}
			}
			else        //向下
			{
				States[1]--;
				if(States[1]==0)
				{
					States[1] = -1;    //没有层
				}
				else
				{
					cout<<"电梯向下运行，将要到达"<<States[1]<<"层！"<<endl<<endl;
					if(States[1]==LowerLayers)    //向下到最底，必定要停，且方向变为向上
					{
						States[2] = 0;      //变方向
						States[0] = 0;
					}
					else
					{
						if(OuterDownButton[States[1]-LowerLayers-1]==0 && DigitalButton[States[1]-LowerLayers-1]==0)
						{
							cout<<"没有乘客在"<<States[1]<<"层上下，电梯继续向下运行!"<<endl<<endl;							
						}
						else
						{
							States[0] = 0;
						}
					}
				}			
			}			
		}		
	}
}

int main()
{
	CElevator myElevator(16,-1);
	myElevator.run();

	return 0;
}