///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QDebug>

#include <complex.h>

#include "dsp/dspengine.h"
#include "dsp/scopevis.h"
#include "util/db.h"
#include "maincore.h"

#include "radioclock.h"
#include "radioclocksink.h"

RadioClockSink::RadioClockSink(RadioClock *radioClock) :
        m_scopeSink(nullptr),
        m_radioClock(radioClock),
        m_channelSampleRate(RADIOCLOCK_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsq(0.0),
        m_magsqSum(0.0),
        m_magsqPeak(0.0),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_data(0),
        m_prevData(0),
        m_sample(0),
        m_lowCount(0),
        m_highCount(0),
        m_periodCount(0),
        m_gotMinuteMarker(false),
        m_second(0),
        m_zeroCount(0)
{
    m_phaseDiscri.setFMScaling(RADIOCLOCK_CHANNEL_SAMPLE_RATE / (2.0f * 20.0/M_PI));
    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

RadioClockSink::~RadioClockSink()
{
}

void RadioClockSink::setScopeSink(ScopeVis* scopeSink)
{
    m_scopeSink = scopeSink;
}

void RadioClockSink::sampleToScope(Complex sample)
{
    if (m_scopeSink)
    {
        Real r = std::real(sample) * SDR_RX_SCALEF;
        Real i = std::imag(sample) * SDR_RX_SCALEF;
        SampleVector m_sampleBuffer1;
        m_sampleBuffer1.push_back(Sample(r, i));
        std::vector<SampleVector::const_iterator> vbegin;
        vbegin.push_back(m_sampleBuffer1.begin());
        m_scopeSink->feed(vbegin, m_sampleBuffer1.end() - m_sampleBuffer1.begin());
        m_sampleBuffer1.clear();
    }
}

void RadioClockSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        c *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f) // interpolate
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else // decimate
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }
}

// Extract binary-coded decimal from time code - LSB first
int RadioClockSink::bcd(int firstBit, int lastBit)
{
    const int vals[] = {1, 2, 4, 8, 10, 20, 40, 80};
    int idx = 0;
    int val = 0;
    for (int i = firstBit; i <= lastBit; i++)
    {
        if (m_timeCode[i]) {
            val += vals[idx];
        }
        idx++;
    }
    return val;
}

// Extract binary-coded decimal from time code - MSB first
int RadioClockSink::bcdMSB(int firstBit, int lastBit)
{
    const int vals[] = {1, 2, 4, 8, 10, 20, 40, 80};
    int idx = 0;
    int val = 0;
    for (int i = lastBit; i >= firstBit; i--)
    {
        if (m_timeCode[i]) {
            val += vals[idx];
        }
        idx++;
    }
    return val;
}

// XOR bits together for parity check
int RadioClockSink::xorBits(int firstBit, int lastBit)
{
    int x = 0;
    for (int i = firstBit; i <= lastBit; i++)
    {
        x ^= m_timeCode[i];
    }
    return x;
}

bool RadioClockSink::evenParity(int firstBit, int lastBit, int parityBit)
{
    return xorBits(firstBit, lastBit) == parityBit;
}

bool RadioClockSink::oddParity(int firstBit, int lastBit, int parityBit)
{
    return xorBits(firstBit, lastBit) != parityBit;
}

// German DCF77
// https://en.wikipedia.org/wiki/DCF77
void RadioClockSink::dcf77()
{
    // DCF77 reduces carrier by -16.5dB
    m_threshold = m_thresholdMovingAverage.asDouble() * m_linearThreshold; // xdB below average
    m_data = m_magsq > m_threshold;

    // Look for minute marker - 59th second carrier is held high
    if ((m_data == 0) && (m_prevData == 1))
    {
        if (   (m_highCount <= RADIOCLOCK_CHANNEL_SAMPLE_RATE * 2)
            && (m_highCount >= RADIOCLOCK_CHANNEL_SAMPLE_RATE * 1.6)
            && (m_lowCount <= RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.3)
            && (m_lowCount >= RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.1)
           )
        {
            qDebug() << "RadioClockSink::dcf77 - Minute marker: (low " << m_lowCount << " high " << m_highCount << ") prev period " << m_periodCount;
            if (getMessageQueueToChannel() && !m_gotMinuteMarker) {
                getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Got minute marker"));
            }
            m_periodCount = 0;
            m_second = 0;
            m_gotMinuteMarker = true;
            m_secondMarkers = 1;
        }

        m_lowCount = 0;
    }
    else if ((m_data == 1) && (m_prevData == 0))
    {
        m_highCount = 0;
    }
    else if (m_data == 1)
    {
        m_highCount++;
    }
    else if (m_data == 0)
    {
        m_lowCount++;
    }

    m_sample = false;
    if (m_gotMinuteMarker)
    {
        m_periodCount++;
        if (m_periodCount == 50)
        {
            // Check we get second marker
            m_secondMarkers += m_data == 0;
            // If we see too many 1s instead of 0s, assume we've lost the signal
            if ((m_second > 10) && (m_secondMarkers / m_second < 0.7))
            {
                qDebug() << "RadioClockSink::dcf77 - Lost lock: " << m_secondMarkers << m_second;
                m_gotMinuteMarker = false;
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
                }
            }
            m_sample = true;
        }
        else if (m_periodCount == 150)
        {
            // Get data for timecode
            m_timeCode[m_second] = !m_data; // No carrier = 1, carrier = 0
            m_sample = true;
        }
        else if (m_periodCount == 950)
        {
            if (m_second == 59)
            {
                // Decode timecode to a time and date
                int minute = bcd(21, 27);
                int hour = bcd(29, 34);
                int day = bcd(36, 41);
                int month = bcd(45, 49);
                int year = 2000 + bcd(50, 57);

                QString parityError;
                if (!evenParity(21, 27, m_timeCode[28])) {
                    parityError = "Minute parity error";
                }
                if (!evenParity(29, 34, m_timeCode[35])) {
                    parityError = "Hour parity error";
                }
                if (!evenParity(36, 57, m_timeCode[58])) {
                    parityError= "Data parity error";
                }

                if (parityError.isEmpty())
                {
                    // Bit 17 indicates CEST rather than CET
                    m_dateTime = QDateTime(QDate(year, month, day), QTime(hour, minute), Qt::OffsetFromUTC, m_timeCode[17] ? 2*3600 : 3600);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("OK"));
                    }
                }
                else
                {
                    m_dateTime = m_dateTime.addSecs(1);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create(parityError));
                    }
                }
                m_second = 0;
            }
            else
            {
                m_second++;
                m_dateTime = m_dateTime.addSecs(1);
            }

            if (getMessageQueueToChannel())
            {
                RadioClock::MsgDateTime *msg = RadioClock::MsgDateTime::create(m_dateTime);
                getMessageQueueToChannel()->push(msg);
            }
        }
        else if (m_periodCount == 1000)
        {
            m_periodCount = 0;
        }
    }
    m_prevData = m_data;
}

// French TDF 162kHz
// https://en.wikipedia.org/wiki/TDF_time_signal
// Uses phase modulation, rather than OOK
void RadioClockSink::tdf(Complex &ci)
{
    // FM demodulation
    double magsqRaw;
    Real deviation;
    Real fmDemod = m_phaseDiscri.phaseDiscriminatorDelta(ci, magsqRaw, deviation);

    // Filter
    m_fmDemodMovingAverage(fmDemod);

    // Ternary encoding
    Real avg = m_fmDemodMovingAverage.asDouble();
    if (avg >= 0.5) {
        m_data = 1;
    } else if (avg <= -0.5) {
        m_data = -1;
    } else {
        m_data = 0;
    }

    // Look for minute marker - 59th second is not phase modulated
    if ((m_data == 1) && (m_prevData == 0))
    {
        if (   (m_zeroCount <= RADIOCLOCK_CHANNEL_SAMPLE_RATE * 2)
            && (m_zeroCount >= RADIOCLOCK_CHANNEL_SAMPLE_RATE * 1)
           )
        {
            qDebug() << "RadioClockSink::tdf - Minute marker: (zero " << m_zeroCount << ") prev period " << m_periodCount;
            if (getMessageQueueToChannel() && !m_gotMinuteMarker) {
                getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Got minute marker"));
            }
            m_periodCount = 0;
            m_second = 0;
            m_gotMinuteMarker = true;
            m_secondMarkers = 1;
        }
    }
    else if ((m_data == 0) && (m_prevData != 0))
    {
        m_zeroCount = 0;
    }
    else if (m_data == 0)
    {
        m_zeroCount++;
    }

    m_sample = false;
    if (m_gotMinuteMarker)
    {
        m_periodCount++;
        if (m_periodCount == 12)
        {
            m_bits[0] = m_data;
            m_sample = true;
        }
        else if (m_periodCount == 12+50)
        {
            m_bits[1] = m_data;
            m_sample = true;
        }
        else if (m_periodCount == 12+100)
        {
            m_bits[2] = m_data;
            m_sample = true;
        }
        else if (m_periodCount == 12+150)
        {
            m_bits[3] = m_data;
            m_sample = true;

            // Check we got second marker
            m_secondMarkers += ((m_bits[0] == 1) && (m_bits[1] == -1));
            // If too many second markers are missing, assume we've lost the signal
            if ((m_second > 10) && (m_secondMarkers / m_second < 0.7))
            {
                qDebug() << "RadioClockSink::tdf - Lost lock: " << m_secondMarkers << m_second;
                m_gotMinuteMarker = false;
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
                }
            }

            // No phase modulation from 50ms to 150ms is 0, pos then neg is 1
            if ((m_bits[2] == 0) && (m_bits[3] == 0)) {
                m_timeCode[m_second] = 0;
            } else if ((m_bits[2] == 1) && (m_bits[3] == -1)) {
                m_timeCode[m_second] = 1;
            } else {
                //qDebug() << "Unexpected modulation " << m_second;
            }
        }
        else if (m_periodCount == 950)
        {
            if (m_second == 59)
            {
                // Decode timecode to time and date
                int minute = bcd(21, 27);
                int hour = bcd(29, 34);
                int day = bcd(36, 41);
                int month = bcd(45, 49);
                int year = 2000 + bcd(50, 57);

                QString parityError;
                if (!evenParity(21, 27, m_timeCode[28])) {
                    parityError = "Minute parity error";
                }
                if (!evenParity(29, 34, m_timeCode[35])) {
                    parityError = "Hour parity error";
                }
                if (!evenParity(36, 57, m_timeCode[58])) {
                    parityError= "Data parity error";
                }

                if (parityError.isEmpty())
                {
                    // Bit 17 indicates CEST rather than CET
                    m_dateTime = QDateTime(QDate(year, month, day), QTime(hour, minute), Qt::OffsetFromUTC, m_timeCode[17] ? 2*3600 : 3600);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("OK"));
                    }
                }
                else
                {
                    m_dateTime = m_dateTime.addSecs(1);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create(parityError));
                    }
                }
                m_second = 0;
            }
            else
            {
                m_second++;
                m_dateTime = m_dateTime.addSecs(1);
            }

            if (getMessageQueueToChannel())
            {
                RadioClock::MsgDateTime *msg = RadioClock::MsgDateTime::create(m_dateTime);
                getMessageQueueToChannel()->push(msg);
            }
        }
        else if (m_periodCount == 1000)
        {
            m_periodCount = 0;
        }
    }
    m_prevData = m_data;
}

// UK MSF 60kHz
// https://www.npl.co.uk/products-services/time-frequency/msf-radio-time-signal/msf_time_date_code
void RadioClockSink::msf60()
{
    m_threshold = m_thresholdMovingAverage.asDouble() * m_linearThreshold; // xdB below average
    m_data = m_magsq > m_threshold;

    // Look for minute marker - 500ms low, then 500ms high
    if ((m_data == 0) && (m_prevData == 1))
    {
        if (   (m_highCount <= RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.6)
            && (m_highCount >= RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.4)
            && (m_lowCount <= RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.6)
            && (m_lowCount >= RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.4)
           )
        {
            qDebug() << "RadioClockSink::msf60 - Minute marker: (low " << m_lowCount << " high " << m_highCount << ") prev period " << m_periodCount;
            if (getMessageQueueToChannel() && !m_gotMinuteMarker) {
                getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Got minute marker"));
            }
            m_periodCount = 0;
            m_second = 1;
            m_gotMinuteMarker = true;
            m_secondMarkers = 1;
        }
        m_lowCount = 0;
    }
    else if ((m_data == 1) && (m_prevData == 0))
    {
        m_highCount = 0;
    }
    else if (m_data == 1)
    {
        m_highCount++;
    }
    else if (m_data == 0)
    {
        m_lowCount++;
    }

    m_sample = false;
    if (m_gotMinuteMarker)
    {
        m_periodCount++;
        if (m_periodCount == 50)
        {
            // Check we get second marker
            m_secondMarkers += m_data == 0;
            // If we see too many 1s instead of 0s, assume we've lost the signal
            if ((m_second > 10) && (m_secondMarkers / m_second < 0.7))
            {
                qDebug() << "RadioClockSink::msf60 - Lost lock: " << m_secondMarkers << m_second;
                m_gotMinuteMarker = false;
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
                }
            }
            m_sample = true;
        }
        else if (m_periodCount == 150)
        {
            // Get data bit A for timecode
            m_timeCode[m_second] = !m_data; // No carrier = 1, carrier = 0
            m_sample = true;
        }
        else if (m_periodCount == 250)
        {
            // Get data bit B for timecode
            m_timeCodeB[m_second] = !m_data;
            m_sample = true;
        }
        else if (m_periodCount == 950)
        {
            if (m_second == 59)
            {
                // Decode timecode to time and date
                int minute = bcdMSB(45, 51);
                int hour = bcdMSB(39, 44);
                int day = bcdMSB(30, 35);
                //int dayOfWeek = bcdMSB(36, 38);
                int month = bcdMSB(25, 29);
                int year = 2000 + bcdMSB(17, 24);

                QString parityError;
                if (!oddParity(39, 51, m_timeCodeB[57])) {
                    parityError = "Hour/minute parity error";
                }
                if (!oddParity(25, 35, m_timeCodeB[55])) {
                    parityError= "Day/month parity error";
                }
                if (!oddParity(17, 24, m_timeCodeB[54])) {
                    parityError = "Hour/minute parity error";
                }

                if (parityError.isEmpty())
                {
                    // Bit 58B indicates BST rather than GMT
                    m_dateTime = QDateTime(QDate(year, month, day), QTime(hour, minute), Qt::OffsetFromUTC, m_timeCodeB[58] ? 1*3600 : 0);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("OK"));
                    }
                }
                else
                {
                    m_dateTime = m_dateTime.addSecs(1);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create(parityError));
                    }
                }
                m_second = 0;
            }
            else
            {
                m_second++;
                m_dateTime = m_dateTime.addSecs(1);
            }

            if (getMessageQueueToChannel())
            {
                RadioClock::MsgDateTime *msg = RadioClock::MsgDateTime::create(m_dateTime);
                getMessageQueueToChannel()->push(msg);
            }
        }
        else if (m_periodCount == 1000)
        {
            m_periodCount = 0;
        }
    }

    m_prevData = m_data;
}

void RadioClockSink::processOneSample(Complex &ci)
{
    Complex ca;

    // Calculate average and peak levels for level meter
    Real re = ci.real() / SDR_RX_SCALEF;
    Real im = ci.imag() / SDR_RX_SCALEF;
    Real magsq = re*re + im*im;
    m_movingAverage(magsq);
    m_thresholdMovingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    // Demodulate
    if (m_settings.m_modulation == RadioClockSettings::DCF77) {
        dcf77();
    } else if (m_settings.m_modulation == RadioClockSettings::TDF) {
        tdf(ci);
    } else {
        msf60();
    }

    // Select signals to feed to scope
    Complex scopeSample;
    switch (m_settings.m_scopeCh1)
    {
    case 0:
        scopeSample.real(ci.real() / SDR_RX_SCALEF);
        break;
    case 1:
        scopeSample.real(magsq * 1e6);
        break;
    case 2:
        scopeSample.real(m_magsq * 1e6);
        break;
    case 3:
        scopeSample.real(m_threshold * 1e6);
        break;
    case 4:
        scopeSample.real(m_fmDemodMovingAverage.asDouble());
        break;
    case 5:
        scopeSample.real(m_data);
        break;
    case 6:
        scopeSample.real(m_sample);
        break;
    case 7:
        scopeSample.real(m_gotMinuteMarker);
        break;
    }
    switch (m_settings.m_scopeCh2)
    {
    case 0:
        scopeSample.imag(ci.imag() / SDR_RX_SCALEF);
        break;
    case 1:
        scopeSample.imag(magsq * 1e6);
        break;
    case 2:
        scopeSample.imag(m_magsq * 1e6);
        break;
    case 3:
        scopeSample.imag(m_threshold * 1e6);
        break;
    case 4:
        scopeSample.imag(m_fmDemodMovingAverage.asDouble());
        break;
    case 5:
        scopeSample.imag(m_data);
        break;
    case 6:
        scopeSample.imag(m_sample);
        break;
    case 7:
        scopeSample.imag(m_gotMinuteMarker);
        break;
    }
    sampleToScope(scopeSample);
}

void RadioClockSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "RadioClockSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) RADIOCLOCK_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void RadioClockSink::applySettings(const RadioClockSettings& settings, bool force)
{
    qDebug() << "RadioClockSink::applySettings:"
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_threshold: " << settings.m_threshold
            << " m_modulation: " << settings.m_modulation
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) RADIOCLOCK_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if ((settings.m_threshold != m_settings.m_threshold) || force)
    {
        m_linearThreshold = CalcDb::powerFromdB(-settings.m_threshold);
    }

    if ((settings.m_modulation != m_settings.m_modulation) || force)
    {
        m_gotMinuteMarker = false;
        m_lowCount = 0;
        m_highCount = 0;
        m_zeroCount = 0;
        m_second = 0;
        if (getMessageQueueToChannel()) {
            getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
        }
    }

    m_settings = settings;
}
