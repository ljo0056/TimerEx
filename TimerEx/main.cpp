// TimerEx.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <stdio.h>

#include "TimerEx.h"
using namespace timer_ex;

int main()
{
    InitializeTimer();

    int count = 0;
    std::function<void(TimerIdEx, void*)> func = [&count](TimerIdEx id, void* ptr)
        {
            printf("Timer Callback Id[%d] Count[%d] ptr[%d]\n", id, count++, (int)ptr);
        };

    TimerIdEx timer_id;
    std::vector<TimerIdEx> vec_id;

    int timner = 3;
    for (int ii = 0; ii < timner; ii++)
    {
        if (CreateTimer(timer_id, 1000, func, (void*)ii))
        {
            break;
        }

        vec_id.push_back(timer_id);
    }

    printf("Create Timer Number [%d]\n", vec_id.size());

    getchar();  // wait

    for (TimerIdEx id : vec_id)
    {
        DeleteTimer(id);
    }

    FinalizeTimer();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
