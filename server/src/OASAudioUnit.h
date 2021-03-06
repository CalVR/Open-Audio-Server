/**
 * @file    OASAudioUnit.h
 * @author  Shreenidhi Chowkwale
 *
 */

#ifndef _OAS_AUDIOUNIT_H_
#define _OAS_AUDIOUNIT_H_

#include <string>
#include "AL/alut.h"

namespace oas
{

/**
 * Contains some basic properties and functions useful for modifying sound in OpenAL
 */
class AudioUnit
{
public:
    /**
     * @brief Set the gain
     * @return True on success, false on failure
     */
    virtual bool setGain(ALfloat gain) = 0;

    /**
     * @brief Set the position
     * @return True on success, false on failure
     */
    virtual bool setPosition(ALfloat x, ALfloat y, ALfloat z) = 0;

    /**
     * @brief Set the velocity
     * @return True on success, false on failure
     */
    virtual bool setVelocity(ALfloat x, ALfloat y, ALfloat z) = 0;

    /**
     * @brief Get the handle for this audio unit
     *
     */
    virtual unsigned int getHandle() const = 0;

    /**
     * @brief Get the current gain
     */
    inline virtual float getGain() const
    {
    	return _gain;
    }

    /**
     * @brief Get the current position, for X, Y, Z
     */
    inline virtual float getPositionX() const
    {
    	return _positionX;
    }
    inline virtual float getPositionY() const
    {
    	return _positionY;
    }
    inline virtual float getPositionZ() const
    {
    	return _positionZ;
    }

    /**
     * @brief Get the current velocity for X, Y, Z
     */
    inline virtual float getVelocityX() const
    {
    	return _velocityX;
    }
    inline virtual float getVelocityY() const
    {
    	return _velocityY;
    }
    inline virtual float getVelocityZ() const
    {
    	return _velocityZ;
    }

    inline virtual bool isValid() const
    {
        return _isValid;
    }

    inline virtual void invalidate()
    {
        _isValid = false;
    }

    /**
     * @brief Get the label for the data entry for the given index
     */
    virtual const char* getLabelForIndex(int index) const = 0;

    /**
     * @brief Get the string for the value of the data entry for the given index
     */
    virtual std::string getStringForIndex(int index) const = 0;

    /**
     * @brief Is this particular audio unit a sound source?
     */
    virtual bool isSoundSource() const = 0;

    virtual ~AudioUnit() { }

protected:

    bool _isValid;
    ALfloat _gain;
    ALfloat _positionX, _positionY, _positionZ;
    ALfloat _velocityX, _velocityY, _velocityZ;

};
}


#endif /* _OAS_AUDIOUNIT_H_*/
