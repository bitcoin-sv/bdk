#pragma once

// RAII style class that 
// Calls ECC_Start if required on construction
// Calls ECC_Stop on destruction
class ecc_guard
{
    bool started_{};

public:
    ecc_guard()
    {
        if(!ECC_IsStarted())
        {
            ECC_Start();
            started_ = true;
        }
    }

    ~ecc_guard()
    {
        try
        {
            if(started_)
                ECC_Stop();
        }
        catch(...){}
    }
    
    ecc_guard(const ecc_guard&) = delete;
    ecc_guard& operator=(const ecc_guard&) = delete;
};

