
//-*-c++-*-
// CoreRanker.h -- Resource library core ranker.
/*
 * Author: Zhiru Zhang 
 * Copyright (C) 2006-2010 AutoESL Design Technologies, Inc.
 * All rights reserved.
 */

#ifndef _PLATFORM_CoreRanker_H
#define _PLATFORM_CoreRanker_H

#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XilinxPlat/CoreInst.h"
#include "llvm/Support/XilinxPlat/TargetPlatform.h"
#else
#include "CoreInst.h"
#include "TargetPlatform.h"
#endif

namespace platform 
{

struct CoreRanker : public std::binary_function<std::shared_ptr<CoreInst>, std::shared_ptr<CoreInst>, bool>
{
    enum MetricType
    {
        DelayOrder = 0,
        IntervalOrder = 1,
        LatencyOrder = 2,
        AreaOrder = 3,
        PowerOrder = 4,
        PreferenceOrder = 5,
        IntervalGTOrder = 6,
        ThroughputOrder = 7,
        HasMetDelayBudgetOrder = 8
    };

    enum OrderType
    {
        EQ,
        LT,
        GT
    };
    
    std::vector<MetricType> mMetrics;
    OperType mOpcode;
    double mDelayBudget;

    CoreRanker(OperType opcode = AnyOperation, double delayBudget = -1) 
                : mOpcode(opcode), mDelayBudget(delayBudget) { } 
    
    virtual ~CoreRanker() { }

    void setDefaultMetrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::PreferenceOrder); // high priority
        mMetrics.push_back(CoreRanker::IntervalOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::AreaOrder);
        mMetrics.push_back(CoreRanker::PowerOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // low priority
    }

    void setJunMaMetrics()
    {
        mMetrics.clear();
        // Add a special metrics for Div when none-pipeline to support Jun's PR
        mMetrics.push_back(CoreRanker::PreferenceOrder); // high priority
        mMetrics.push_back(CoreRanker::IntervalGTOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::AreaOrder);
        mMetrics.push_back(CoreRanker::PowerOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // low priority
    }

    void setPipeYes_iiYes_relaxiiYes_metrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::ThroughputOrder); // high priority
        mMetrics.push_back(CoreRanker::HasMetDelayBudgetOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // when II and Latency are tied
    }

    void setPipeYes_iiYes_relaxiiNo_metrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::IntervalOrder); // high priority
        mMetrics.push_back(CoreRanker::HasMetDelayBudgetOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // when II and Latency are tied
    }

    void setPipeYes_iiNo_relaxiiYes_metrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::ThroughputOrder); // high priority
        mMetrics.push_back(CoreRanker::HasMetDelayBudgetOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // when II and Latency are tied
    }

    void setPipeYes_iiNo_relaxiiNo_metrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::ThroughputOrder); // high priority
        mMetrics.push_back(CoreRanker::HasMetDelayBudgetOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // when II and Latency are tied
    }

    void setPipeNo_iiNo_relaxiiNo_metrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::HasMetDelayBudgetOrder); // high priority
        mMetrics.push_back(CoreRanker::ThroughputOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // when II and Latency are tied
    }

    template <class T>
    OrderType compareInt(T a, T b)
    {
        if (a < b)
            return LT;
        else if (a > b)
            return GT;
        else
            return EQ;
    }

    template <class T>
    OrderType compareFloat(T a, T b)
    {
        if ((a + 1e-10) < b)
            return LT;
        else if (a > (b + 1e-10))
            return GT;
        else
            return EQ;
    }

    // seems no use, delete by changhon
    //static OperType getCoreOper(std::shared_ptr<const CoreInst> core, OperType opcode)
    //{
    //    return opcode;
    //}
    
    OrderType rankByDelay(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        double delay1 = c1->getDelay(mOpcode);
        double delay2 = c2->getDelay(mOpcode); 

        return compareFloat<double>(delay1, delay2);
    }

    OrderType rankByInterval(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        unsigned ii1 = c1->getPipeInterval(mOpcode);
        unsigned ii2 = c2->getPipeInterval(mOpcode);

        // II=0 means the component is not pipelined.
        if (ii1 == 0 && ii2 > 0)
            return GT;
        if (ii2 == 0 && ii1 > 0)
            return LT;

        return compareInt<unsigned>(ii1, ii2);
    }

    OrderType rankByHasMetDelayBudgetOrder(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        unsigned hasMetDelayBudget1 = c1->getDelay(mOpcode) <= mDelayBudget ? 1 : 0;
        unsigned hasMetDelayBudget2 = c2->getDelay(mOpcode) <= mDelayBudget ? 1 : 0;
    
        return compareInt<unsigned>(hasMetDelayBudget1, hasMetDelayBudget2);
    }

    OrderType rankByThroughput(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        double delay1 = c1->getDelay(mOpcode);
        double delay2 = c2->getDelay(mOpcode);
        // not divide by zero
        assert(delay1);
        assert(delay2);

        delay1 = (mDelayBudget > 0 && mDelayBudget > delay1) ? mDelayBudget : delay1;
        delay2 = (mDelayBudget > 0 && mDelayBudget > delay2) ? mDelayBudget : delay2;
        
        unsigned ii1 = c1->getPipeInterval(mOpcode);
        unsigned ii2 = c2->getPipeInterval(mOpcode); 
        
        // II=0 means the component is not pipelined.
        if (ii1 == 0 && ii2 > 0)
            return LT;
        if (ii2 == 0 && ii1 > 0)
            return GT; 
        if (ii1 == 0 && ii2 == 0)
            return EQ;

        // fix divide by zeros, not functional
        if (delay1 == 0 && delay2 > 0)
            return LT;
        if (delay2 == 0 && delay1 > 0)
            return GT; 
        if (delay1 == 0 && delay2 == 0)
            return EQ;

        double throughput1 = 1/delay1 * 1/(double)ii1;
        double throughput2 = 1/delay2 * 1/(double)ii2;

        return compareFloat<double>(throughput1, throughput2);
    }

    OrderType rankByLatency(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        unsigned lat1 = c1->getPipeLatency(mOpcode);
        unsigned lat2 = c2->getPipeLatency(mOpcode);

        return compareInt<unsigned>(lat1, lat2);
    }

    OrderType rankByArea(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        const ResUsageMap& res1 = c1->getResourceUsage();
        const ResUsageMap& res2 = c2->getResourceUsage();

        //TargetPlatform* platform = c1->getParent()->getParent();
        TargetPlatform* platform = GetTargetPlatform();
        double scaler = 1000.0;
        double cost1 = platform->getNormalizedCost(res1) * scaler;
        double cost2 = platform->getNormalizedCost(res2) * scaler;

        // Handle the abnormalies, e.g., zero-cost component.
        if (cost1 <= 1e-20 && cost2 >= 1e-5) 
            return GT;
        if (cost2 <= 1e-20 && cost1 >= 1e-5) 
            return LT;
            
        // FIXME: Use the true cost.
        return compareFloat<double>(cost1, cost2);
    }
    
    OrderType rankByPower(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        double dynamic1 = c1->getDynamicPower(mOpcode);
        double static1 = c1->getLeakagePower(mOpcode);
        double dynamic2 = c2->getDynamicPower(mOpcode);
        double static2 = c2->getLeakagePower(mOpcode);

        double power1 = dynamic1 + static1;
        double power2 = dynamic2 + static2;

        return compareFloat<double>(power1, power2);
    }
    
    OrderType rankByPreference(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        float p1 = c1->getPreference(mOpcode);
        float p2 = c2->getPreference(mOpcode);

        return compareFloat<float>(p1, p2);
    }
    
    bool operator() (std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        assert(c1 && c2);
        
        for (auto& it : mMetrics)
        {
            // Preference.
            if (it == PreferenceOrder)
            {
                OrderType order = rankByPreference(c1, c2);            
                if (order == GT)
                    return true;
                else if (order == LT)
                    return false;
            }

            // Interval.
            if (it == IntervalOrder)
            {
                OrderType order = rankByInterval(c1, c2);
                if (order == LT)
                    return true;
                else if (order == GT)
                    return false;
            }
            
            if (it == IntervalGTOrder)
            {
                OrderType order = rankByInterval(c1, c2);
                if (order == LT)
                    return false;
                else if (order == GT)
                    return true;
            }
            
            // Latency.
            if (it == LatencyOrder)
            {
                OrderType order = rankByLatency(c1, c2);            
                if (order == LT)
                    return true;
                else if (order == GT)
                    return false;
            }

            // Area.
            if (it == AreaOrder)
            {
                OrderType order = rankByArea(c1, c2);
                if (order == LT)
                    return true;
                else if (order == GT)
                    return false;
            }

            // Power.
            if (it == PowerOrder)
            {
                OrderType order = rankByPower(c1, c2);
                if (order == LT)
                    return true;
                else if (order == GT)
                    return false;
            }

            // Delay.
            if (it == DelayOrder)
            {
                OrderType order = rankByDelay(c1, c2);
                if (order == LT)
                    return true;
                else if (order == GT)
                    return false;
            }

            // Throughput.
            if (it == ThroughputOrder)
            {
                OrderType order = rankByThroughput(c1, c2);
                if (order == GT)
                    return true;
                else if (order == LT)
                    return false;  
            }  

            // HasMetDelayBudget.
            if (it == HasMetDelayBudgetOrder)
            {
                OrderType order = rankByHasMetDelayBudgetOrder(c1, c2);
                if (order == GT)
                        return true;
                else if (order == LT)
                    return false;
            }
        }            

        return (c1->getId() < c2->getId());
    }    
};

}

#endif


