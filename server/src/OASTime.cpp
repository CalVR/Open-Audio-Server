#include <OASTime.h>

using namespace oas;

void Time::update()
{
    clock_gettime(CLOCK_MONOTONIC, &_time);
}

void Time::reset()
{
	_time.tv_sec = 0;
	_time.tv_nsec = 0;
}

double Time::asDouble() const
{
    return (getSeconds() +  ((double) getNanoseconds() / OAS_BILLION));
}

Time& Time::operator=(const Time &rhs)
{
    if (this != &rhs)
    {
        _time.tv_sec = rhs.getSeconds();
        _time.tv_nsec = rhs.getNanoseconds();
    }

    return *this;
}

Time& Time::operator +=(const Time &rhs)
{
    long int nanoseconds = _time.tv_nsec + rhs.getNanoseconds();

    _time.tv_nsec = nanoseconds % OAS_BILLION;
    _time.tv_sec += rhs.getSeconds() + (nanoseconds / OAS_BILLION);

    return *this;
}

Time& Time::operator-=(const Time &rhs)
{
	//std::cerr << "Time subtraction: " << _time.tv_sec << ", " << _time.tv_nsec << " - " << rhs.getSeconds() << ", " << rhs.getNanoseconds() << std::endl;

    if (_time.tv_nsec < rhs.getNanoseconds())
    {
        _time.tv_sec -= 1;
        _time.tv_nsec += OAS_BILLION;
    }

    _time.tv_nsec -= rhs.getNanoseconds();
    _time.tv_sec -= rhs.getSeconds();

    //std::cerr << "Results in: " << getSeconds() << ", " << getNanoseconds() << std::endl;
    return *this;
}

const Time& Time::operator +(const Time &other) const
{
    return Time(*this) += other;
}

const Time& Time::operator -(const Time &other) const
{
    return Time(*this) -= other;
}

bool Time::operator >(const Time &other) const
{
	if (getSeconds() > other.getSeconds()
		|| getSeconds() == other.getSeconds() && getNanoseconds() > other.getNanoseconds())
		return true;
	else
		return false;
}

Time::Time(double floatingRepresentationInSeconds)
{
    _time.tv_sec = (long int) floatingRepresentationInSeconds;
    _time.tv_nsec = (long int) ((floatingRepresentationInSeconds - _time.tv_sec) * OAS_BILLION);
}

Time::Time()
{
	reset();
}

