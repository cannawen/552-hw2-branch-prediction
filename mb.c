#include <stdio.h>

main(){
	int i;
	for(i=0;i<100000;i++) // 0/1 incorrect
	{	
		int y=0,z=0;
		if(i%3==0)// 1/3 incorrect
			y=1;
		if(i%5==0)// 1/5 incorrect
			z=1;
		if(y&z)// 1/15 incorrct
			y++;
	}//total: 0/15 + 5/15 + 3/15 + 1/15 = 9/60 incorrect = 15%
}