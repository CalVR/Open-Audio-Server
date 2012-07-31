#include "OASAudioHandler.h"

using namespace oas;

// public, static
AudioHandler& AudioHandler::getInstance()
{
    static AudioHandler instance;

    return instance;
}

// public
bool AudioHandler::initialize(std::string const& deviceString)
{
    // If we have a specific device we're going to try to use,
    // we have to set up OpenAL manually
    if (0 < deviceString.length())
    {
        // Try to Init ALUT
        if (!alutInitWithoutContext(NULL, NULL))
        {
            ALenum error = alutGetError();
            oas::Logger::errorf("AudioHandler - %s", alutGetErrorString(error));
            return false;
        }

        oas::Logger::logf("AudioHandler - Opening audio device \"%s\"", deviceString.c_str());
        // Try to open the device
        AudioHandler::_device = alcOpenDevice(deviceString.c_str());
        if (!AudioHandler::_device)
        {
            oas::Logger::errorf("AudioHandler - Failed to open device \"%s\"",
                                deviceString.c_str());
            return false;
        }

        // Try to create the context
        AudioHandler::_context = alcCreateContext(AudioHandler::_device, NULL);
        if (!AudioHandler::_context)
        {
            oas::Logger::errorf("AudioHandler - Failed to create audio context for device \"%s\"",
                                deviceString.c_str());
            alcCloseDevice(AudioHandler::_device);
            return false;
        }

        // Try to make the context current
        if (!alcMakeContextCurrent(AudioHandler::_context))
        {
            oas::Logger::errorf("AudioHandler - Failed to make context current for device \"%s\"",
                                deviceString.c_str());
            alcDestroyContext(AudioHandler::_context);
            alcCloseDevice(AudioHandler::_device);
            return false;
        }
    }
    // Else, let ALUT automatically set up our OpenAL context and devices, using defaults
    else
    {
        if (!alutInit(NULL, NULL))
        {
            ALenum error = alutGetError();
            oas::Logger::errorf("AudioHandler - %s", alutGetErrorString(error));
            return false;
        }
    }

    if (0 < deviceString.length())
        oas::Logger::logf("AudioHandler initialized with device \"%s\"", deviceString.c_str());
    else
        oas::Logger::logf("AudioHandler initialized! Using system default device to drive sound.");

    AudioHandler::_deviceString = deviceString;
    AudioHandler::_recentSource = NULL;
    _setRecentlyModifiedAudioUnit(&AudioListener::getInstance());

    return true;
}

// public
void AudioHandler::release()
{
    // Release the sources
    SourceMapIterator sIter;

    for (sIter = _sourceMap.begin(); sIter != _sourceMap.end(); sIter++)
    {
        if (sIter->second)
            delete sIter->second;
    }
    
    _sourceMap.clear();
    oas::AudioSource::resetSources();

    // Release the buffers
    BufferMapIterator bIter;

    for (bIter = _bufferMap.begin(); bIter != _bufferMap.end(); bIter++)
    {
        if (bIter->second)
            delete bIter->second;
    }

    _bufferMap.clear();

    AudioListener::getInstance().setGain(1);
    AudioListener::getInstance().setPosition(0, 0, 0);
    AudioListener::getInstance().setOrientation(0, 0, -1, 0, 1, 0);
    AudioListener::getInstance().setVelocity(0, 0, 0);
    _setRecentlyModifiedAudioUnit(&AudioListener::getInstance());

    if (0 < AudioHandler::_deviceString.length())
    {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(AudioHandler::_context);
        alcCloseDevice(AudioHandler::_device);
    }
    // Let ALUT do any remaining cleanup to destroy the context
    if (!alutExit())
    {
        ALenum error = alutGetError();
        oas::Logger::errorf("AudioHandler - %s", alutGetErrorString(error));
    }
}

// public
ALuint AudioHandler::getBuffer(const std::string& filename)
{
    if (filename.empty())
    {
        return AL_NONE;
    }

    // See if buffer with that filename exists already
    BufferMapIterator iterator = _bufferMap.find(filename);

    // If something was found 
    if (_bufferMap.end() != iterator)
    {
        // If the found buffer was valid
        if(iterator->second && iterator->second->isValid())
        {
            return iterator->second->getHandle();
        }
        // Else remove the previous entry
        else
        {
            if (iterator->second)
            {
                delete iterator->second;
            }
            _bufferMap.erase(iterator);
        }
    }

    // Make a new buffer
    AudioBuffer *newBuffer = new AudioBuffer(filename);
    if (!newBuffer->isValid())
    {
        delete newBuffer;
        oas::Logger::warnf("AudioHandler - Could not create a sound buffer for \"%s\"",
                            filename.c_str());
        return AL_NONE;
    }

    _bufferMap.insert(BufferPair(newBuffer->getFilename().c_str(), newBuffer));
    return newBuffer->getHandle();
}

// private, static
AudioSource* AudioHandler::_getSource(const ALuint sourceHandle)
{
    // Short circuit the map lookup. See if handle matches the most recently looked up source
    if (_recentSource && (sourceHandle == _recentSource->getHandle()))
    {
        return _recentSource;
    }

    // Else, we have to look through the map
    SourceMapConstIterator iterator = _sourceMap.find(sourceHandle);

    if (_sourceMap.end() != iterator && iterator->second)
    {
        _recentSource = iterator->second;
        return iterator->second;
    }
    else
    {
        return NULL;
    }
}

// private, static
void AudioHandler::_clearRecentlyModifiedAudioUnit()
{
    _recentlyModifiedAudioUnit = NULL;
}

// private, static
void AudioHandler::_setRecentlyModifiedAudioUnit(const AudioUnit *unit)
{
    _recentlyModifiedAudioUnit = unit;
}

// public
const AudioUnit* AudioHandler::getRecentlyModifiedAudioUnit()
{
    AudioUnit *retval = NULL;

    if (_recentlyModifiedAudioUnit)
    {
        if (_recentlyModifiedAudioUnit->isSoundSource())
        {
            // We create a duplicate copy of the recently modified source
            retval = new AudioSource(*(static_cast<const AudioSource *>(_recentlyModifiedAudioUnit)));
        }
        else
        {
            // We create a duplicate copy of the listener
            retval = new AudioListener(AudioListener::getInstance());
        }

        // The duplicate is invalidated, so that it cannot be used to modify the sound state.
        // This is in addition to the inherent constness of the returned pointer.
        retval->invalidate();
        // Set the recently modified source to NULL
        _recentlyModifiedAudioUnit = NULL;
    }
    
    return retval;
}

// public
const AudioListener* AudioHandler::getListenerCopy()
{
	AudioListener *copy = new AudioListener(AudioListener::getInstance());
	copy->invalidate();

	return copy;
}

void AudioHandler::populateQueueWithUpdatedSources(std::queue <const AudioUnit*> &sources)
{
    SourceMapIterator iterator;

    for (iterator = AudioHandler::_sourceMap.begin(); iterator != AudioHandler::_sourceMap.end(); iterator++)
    {
        if (!iterator->second)
            continue;

        if (iterator->second->update())
        {
            AudioSource *copy = new AudioSource(*iterator->second);
            copy->invalidate();
            sources.push(copy);
        }
    }
}


// public
int AudioHandler::createSource(ALuint buffer)
{
    if (AL_NONE == buffer)
    {
        return -1;
    }

    AudioSource *newSource = new AudioSource(buffer);

    if (newSource->isValid())
    {
        _sourceMap.insert(SourcePair(newSource->getHandle(), newSource));
        _recentSource = newSource;
        newSource->setRolloffFactor(_defaultRolloff);
        newSource->setReferenceDistance(_defaultReferenceDistance);
        _setRecentlyModifiedAudioUnit(newSource);
        return newSource->getHandle();
    }
    else
    {
        delete newSource;
        return -1;
    }
}

// public
int AudioHandler::createSource(const std::string& filename)
{
    ALuint buffer = AudioHandler::getBuffer(filename);
    return AudioHandler::createSource(buffer);
}

// public
int AudioHandler::createSource(ALint waveShape, ALfloat frequency, ALfloat phase, ALfloat duration)
{
    // First, a new buffer must be created with the specified waveform.
    AudioBuffer *newBuffer = new AudioBuffer(waveShape, frequency, phase, duration);

    // Check if buffer created successfully
    if (!newBuffer->isValid())
    {
        delete newBuffer;
        return -1;
    }

    // Create a new source with the new buffer
    AudioSource *newSource = new AudioSource(newBuffer->getHandle());

    // If new source created successfully
    if (newSource->isValid())
    {
        // Add buffer to the buffer map
        _bufferMap.insert(BufferPair(newBuffer->getFilename().c_str(), newBuffer));
        // Add source to the sourcemap
        _sourceMap.insert(SourcePair(newSource->getHandle(), newSource));
        _recentSource = newSource;
        _setRecentlyModifiedAudioUnit(newSource);
        return newSource->getHandle();
    }
    else
    {
        delete newBuffer;
        delete newSource;
        return -1;
    }
}

// public
void AudioHandler::deleteSource(const ALuint sourceHandle)
{
	/*
	 * Strategy:
	 * The memory allocated for an audio source is not deleted immediately. This is to give time
	 * for other threads to be notified that this particular audio source has been deleted, and
	 * prevent access to invalid memory.
	 */
    static std::queue<AudioSource*> lazyDeletionQueue;

    // Find the source in the map, and if found, queue it up for deletion
    SourceMapIterator iterator = AudioHandler::_sourceMap.find(sourceHandle);

    _clearRecentlyModifiedAudioUnit();

    if (AudioHandler::_sourceMap.end() != iterator)
    {
        if (iterator->second)
        {
            // Let the source know that it is to be deleted
            // Note that the AudioSource is not explicitly deleted yet - only the internal state
        	// is notified that it is to be deleted
            if (!iterator->second->deleteSource())
            {
                oas::Logger::warnf("AudioHandler: Deletion of sound source failed!");
            }

            // Push the pointer to the source onto the lazy deletion queue
            lazyDeletionQueue.push(iterator->second);
            // Update the recently modified source
            _setRecentlyModifiedAudioUnit(iterator->second);
        }
        _sourceMap.erase(iterator);
    }

    // If the lazy deletion queue hits a size greater than 5, remove an entry
    if (lazyDeletionQueue.size() > 5)
    {
        // Delete the Source at the head of the queue
        delete lazyDeletionQueue.front();
        // Pop the pointer to freed memory off of the queue
        lazyDeletionQueue.pop();
    }
}

// public
void AudioHandler::playSource(const ALuint sourceHandle)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();
    
    if (source)
    {
        if (source->play())
            _setRecentlyModifiedAudioUnit(source);
    }
}

// public
void AudioHandler::stopSource(const ALuint sourceHandle)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();
    
    if (source)
    {
        if (source->stop())
            _setRecentlyModifiedAudioUnit(source);
    }
}

// public
void AudioHandler::pauseSource(const ALuint sourceHandle)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();

    if (source)
    {
        if (source->pause())
            _setRecentlyModifiedAudioUnit(source);
    }
}

// public
void AudioHandler::setDefaultRolloffFactor(const ALfloat rolloff)
{
    _defaultRolloff = rolloff;
}

// public
void AudioHandler::setDefaultReferenceDistance(const ALfloat referenceDistance)
{
    _defaultReferenceDistance = referenceDistance;
}

// public
void AudioHandler::setSourcePlaybackPosition(const ALuint sourceHandle, const ALfloat seconds)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();

    if (source)
    {
        if (source->setPlaybackPosition(seconds))
            _setRecentlyModifiedAudioUnit(source);
    }

}

// public
void AudioHandler::setSourcePosition(const ALuint sourceHandle, const ALfloat x, const ALfloat y, const ALfloat z)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();
    
    if (source)
    {
        if (source->setPosition(x, y, z))
            _setRecentlyModifiedAudioUnit(source);
    }

}

// public
void AudioHandler::setSourceGain(const ALuint sourceHandle, const ALfloat gain)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();
    
    if (source)
    {
        if (source->setGain(gain))
            _setRecentlyModifiedAudioUnit(source);
    }

}

// public
void AudioHandler::setSourceLoop(const ALuint sourceHandle, const ALint isLoop)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();
    
    if (source)
    {
        if (source->setLoop(isLoop))
            _setRecentlyModifiedAudioUnit(source);
    }

}

// public
void AudioHandler::setSourceVelocity(const ALuint sourceHandle, const ALfloat x, const ALfloat y, const ALfloat z)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();
    
    if (source)
    {
        if (source->setVelocity(x, y, z))
            _setRecentlyModifiedAudioUnit(source);
    }
}

// public
void AudioHandler::setSourceSpeed(const ALuint sourceHandle, const ALfloat speed)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();
    
    if (source)
    {
        if (source->setVelocity( speed * source->getDirectionX(),
                                 speed * source->getDirectionY(),
                                 speed * source->getDirectionZ()))
        {
            _setRecentlyModifiedAudioUnit(source);
        }
    }

}

// public
void AudioHandler::setSourceDirection(const ALuint sourceHandle, const ALfloat x, const ALfloat y, const ALfloat z)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();
    
    if (source)
    {
        if (source->setDirection(x, y, z))
            _setRecentlyModifiedAudioUnit(source);
    }

}

// public
void AudioHandler::setSourceDirection(const ALuint sourceHandle, const ALfloat angleInRadians)
{
    AudioHandler::setSourceDirection( sourceHandle, 
                                      sin(angleInRadians),
                                      0.0,
                                      cos(angleInRadians));
}

// public
void AudioHandler::setSourcePitch(const ALuint sourceHandle, const ALfloat pitchFactor)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);

    _clearRecentlyModifiedAudioUnit();

    if (source)
    {
        if (source->setPitch(pitchFactor))
            _setRecentlyModifiedAudioUnit(source);
    }
}

// public
void AudioHandler::setSourceFade(const ALuint sourceHandle, const ALfloat fadeToGainValue, const ALfloat durationInSeconds)
{
	AudioSource *source = AudioHandler::_getSource(sourceHandle);

	_clearRecentlyModifiedAudioUnit();

	if (source)
	{
		if (source->setFade(fadeToGainValue, durationInSeconds))
			_setRecentlyModifiedAudioUnit(source);
	}
}

// public
int AudioHandler::getSourceState(const ALuint sourceHandle)
{
    AudioSource *source = AudioHandler::_getSource(sourceHandle);
    int state = 0;

    _clearRecentlyModifiedAudioUnit();

    if (source)
    {
        source->update(true);
        state = source->getState();
        _setRecentlyModifiedAudioUnit(source);
    }

    return state;
}

void AudioHandler::setListenerGain(const ALfloat gain)
{
    _clearRecentlyModifiedAudioUnit();

    if (AudioListener::getInstance().setGain(gain))
        _setRecentlyModifiedAudioUnit(&AudioListener::getInstance());
}

void AudioHandler::setListenerPosition(const ALfloat x, const ALfloat y, const ALfloat z)
{
    _clearRecentlyModifiedAudioUnit();

    if (AudioListener::getInstance().setPosition(x, y, z))
        _setRecentlyModifiedAudioUnit(&AudioListener::getInstance());
}

void AudioHandler::setListenerVelocity(const ALfloat x, const ALfloat y, const ALfloat z)
{
    _clearRecentlyModifiedAudioUnit();

    if (AudioListener::getInstance().setVelocity(x, y, z))
        _setRecentlyModifiedAudioUnit(&AudioListener::getInstance());
}

void AudioHandler::setListenerOrientation(const ALfloat atX, const ALfloat atY, const ALfloat atZ,
                                   const ALfloat upX, const ALfloat upY, const ALfloat upZ)
{
    _clearRecentlyModifiedAudioUnit();

    if (AudioListener::getInstance().setOrientation(atX, atY, atZ, upX, upY, upZ))
        _setRecentlyModifiedAudioUnit(&AudioListener::getInstance());
}

void AudioHandler::setSoundRenderingParameters(const ALuint whichParameter, ALfloat value)
{
    _clearRecentlyModifiedAudioUnit();

    bool result = false;

    switch (whichParameter)
    {
        case SPEED_OF_SOUND:
            result = AudioListener::getInstance().setSpeedOfSound(value);
            break;
        case DOPPLER_FACTOR:
            result = AudioListener::getInstance().setDopplerFactor(value);
            break;
        case DEFAULT_ROLLOFF:
            setDefaultRolloffFactor(value);
            break;
        case DEFAULT_REFERENCE_DISTANCE:
            setDefaultReferenceDistance(value);
            break;
        default:
            oas::Logger::warnf("AudioHandler - Unknown sound rendering parameter %d was specified", whichParameter);
            break;
    }

    if (result)
        _setRecentlyModifiedAudioUnit(&AudioListener::getInstance());
}

// private constructor
AudioHandler::AudioHandler() :
        _recentSource(NULL),
        _recentlyModifiedAudioUnit(NULL),
        _device(NULL),
        _context(NULL),
        _defaultRolloff(1),
        _defaultReferenceDistance(1)
{

}
