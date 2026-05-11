#ifndef LAB5_2_CTRL_PID_H
#define LAB5_2_CTRL_PID_H

class PidController52
{
public:
    void Init(float kp, float ki, float kd, float limitAbs, float initialOutput);
    void Reset(float initialOutput);
    void Configure(float kp, float ki, float kd, float limitAbs);
    float Step(float error, float dtSeconds);

private:
    float clamp(float value, float limitAbs) const;

    float kp;
    float ki;
    float kd;
    float limitAbs;

    float integral;
    float prevError;
    float prevDerivative;
    bool initialized;
};

#endif
