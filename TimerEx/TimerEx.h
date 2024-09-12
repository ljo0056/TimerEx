#pragma once

#include <functional>

//////////////////////////////////////////////////////////////////////////
///  @file    TimerEx.h
///  @date    2019/07/12
///  @author  Lee Jong Oh
///  @brief   Windows, Linux 에서 사용하는 Timer API
///           Windows 는 MMTimer 를 이용하여 구현
///           Linux 는 Signal 방식으로 구현

namespace timer_ex
{
    typedef int TimerIdEx;

    ///  @brief      Application 초기에 호출 한다.
    ///  @return     성공 시에 0, 실패 시에 1 이상의 값을 return 한다.
    int InitializeTimer();
    ///  @brief      Application 종료 직전에 호출 한다.
    ///              내부적으로 Timer 객체들을 모두 제거하며 따로 호출하지 않아도 내부 소멸자에서 호출한다.
    ///              명시적으로 호출이 필요 할 시에 사용 한다.
    ///  @return     성공 시에 0, 실패 시에 1 이상의 값을 return 한다.
    int FinalizeTimer();

    ///  @brief      Timer 객체를 생성하고 설정된 시간 마다 Callback 함수를 호출 한다.
    ///  @param id[out] : Timer 객체를 식별하는 id 값을 받아온다.
    ///  @param ms[in] : Timer 의 이벤트를 받을 시간을 설정 한다. 단위는 밀리세컨드
    ///  @param func[in] : ms 의 설정된 시간마다 호출되는 Callback 함수
    ///  @param ptr[in] : func 의 ptr 인자로 넘어가는 유저 정의 값을 설정 한다.
    ///  @return     성공 시에 0, 실패 시에 1 이상의 값을 return 한다.
    int CreateTimer(TimerIdEx& id, int ms, std::function<void(TimerIdEx id, void* ptr)> func, void* ptr);

    ///  @brief      Timer 객체를 삭제한다.
    ///  @param id[in] : CreateTimer api 에서 얻어온 id 값
    ///  @return     성공 시에 0, 실패 시에 1 이상의 값을 return 한다.
    int DeleteTimer(const TimerIdEx& id);
};

// Sample code
#if 0

#include <vector>
#include <stdio.h>
#include "TimerEx.h"

using namespace timer_ex;

int main()
{
    InitializeTimer();

    std::function<void(TimerIdEx, void*)> func = [](TimerIdEx id, void* ptr) {
        // do something...
    };

    TimerIdEx timer_id;
    std::vector<TimerIdEx> vec_id;

    for (int ii = 0; ii < 10; ii++)
    {
        if (CreateTimer(timer_id, 100, func, (void*)ii))
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
#endif

