#include <stdio.h>

main(){
	int i,y=0;
	for(i=0;i<100000;i++) //100% correct
	{	
		if(i%3==0)//66% correct
			y++;
	}//total: 5/6 correctness
}