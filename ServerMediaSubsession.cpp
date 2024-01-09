/*
 * MIT License
 *
 * Copyright (c) 2020 vzvca
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sstream>

// live555
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <Base64.hh>

// project
#include "ServerMediaSubsession.h"
#include "VideoDeviceSource.h"

// ---------------------------------
//   BaseServerMediaSubsession
// ---------------------------------
FramedSource* BaseServerMediaSubsession::createSource(UsageEnvironment& env, FramedSource * videoES, int format)
{
  FramedSource* source = NULL;
  /* VCA - don't know how to handle other formats
  switch (format)
    {
    case FFMPEG_PIX_FMT_H264 : source = H264VideoStreamDiscreteFramer::createNew(env, videoES); break;
    }
  */

  source = H264VideoStreamDiscreteFramer::createNew(env, videoES);
  return source;
}

RTPSink*  BaseServerMediaSubsession::createSink(UsageEnvironment& env, Groupsock * rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, int format)
{
  RTPSink* videoSink = NULL;
  /* VCA - don't know how to handle other formats
  switch (format)
    {
    case FFMPEG_PIX_FMT_H264 : videoSink = H264VideoRTPSink::createNew(env, rtpGroupsock,rtpPayloadTypeIfDynamic); break;
    }
  */
  videoSink = H264VideoRTPSink::createNew(env, rtpGroupsock,rtpPayloadTypeIfDynamic);
  return videoSink;
}

char const* BaseServerMediaSubsession::getAuxLine(VideoDeviceSource* source,unsigned char rtpPayloadType)
{
  const char* auxLine = NULL;
  if (source)
    {
      std::ostringstream os; 
      os << "a=fmtp:" << int(rtpPayloadType) << " ";				
      os << source->getAuxLine();				
      os << "\r\n";				
      auxLine = strdup(os.str().c_str());
    } 
  return auxLine;
}

// -----------------------------------------
//    ServerMediaSubsession for Multicast
// -----------------------------------------
MulticastServerMediaSubsession* MulticastServerMediaSubsession::createNew(UsageEnvironment& env
									  , struct in_addr destinationAddress
									  , Port rtpPortNum, Port rtcpPortNum
									  , int ttl
									  , unsigned char rtpPayloadType
									  , StreamReplicator* replicator
									  , int format) 
{ 
  // Create a source
  FramedSource* source = replicator->createStreamReplica();			
  FramedSource* videoSource = createSource(env, source, format);

  // Create RTP/RTCP groupsock
  Groupsock* rtpGroupsock = new Groupsock(env, destinationAddress, rtpPortNum, ttl);
  Groupsock* rtcpGroupsock = new Groupsock(env, destinationAddress, rtcpPortNum, ttl);

  // Create a RTP sink
  RTPSink* videoSink = createSink(env, rtpGroupsock, rtpPayloadType, format);

  // Create 'RTCP instance'
  const unsigned maxCNAMElen = 100;
  unsigned char CNAME[maxCNAMElen+1];
  gethostname((char*)CNAME, maxCNAMElen);
  CNAME[maxCNAMElen] = '\0'; 
  RTCPInstance* rtcpInstance = RTCPInstance::createNew(env, rtcpGroupsock,  500, CNAME, videoSink, NULL);

  // Start Playing the Sink
  videoSink->startPlaying(*videoSource, NULL, NULL);
	
  return new MulticastServerMediaSubsession(replicator, videoSink, rtcpInstance);
}
		
char const* MulticastServerMediaSubsession::sdpLines() 
{
  if (m_SDPLines.empty())
    {
      // Ugly workaround to give SPS/PPS that are get from the RTPSink 
      m_SDPLines.assign(PassiveServerMediaSubsession::sdpLines());
      m_SDPLines.append(getAuxSDPLine(m_rtpSink,NULL));
    }
  return m_SDPLines.c_str();
}

char const* MulticastServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink,FramedSource* inputSource)
{
  return this->getAuxLine(dynamic_cast<VideoDeviceSource*>(m_replicator->inputSource()), rtpSink->rtpPayloadType());
}
		
// -----------------------------------------
//    ServerMediaSubsession for Unicast
// -----------------------------------------
UnicastServerMediaSubsession* UnicastServerMediaSubsession::createNew(UsageEnvironment& env, StreamReplicator* replicator, int format) 
{ 
  return new UnicastServerMediaSubsession(env,replicator,format);
}
					
FramedSource* UnicastServerMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate)
{
  FramedSource* source = m_replicator->createStreamReplica();
  return createSource(envir(), source, m_format);
}
		
RTPSink* UnicastServerMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock,  unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource)
{
  return createSink(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic, m_format);
}
		
char const* UnicastServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink,FramedSource* inputSource)
{
  return this->getAuxLine(dynamic_cast<VideoDeviceSource*>(m_replicator->inputSource()), rtpSink->rtpPayloadType());
}
