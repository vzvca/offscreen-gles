/*
 * MIT License
 *
 * Copyright (c) 2024 vzvca
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

#ifndef SERVER_MEDIA_SUBSESSION
#define SERVER_MEDIA_SUBSESSION

#include <string>

// live555
#include <liveMedia.hh>

//forward declarations
class VideoDeviceSource;

// ---------------------------------
//   BaseServerMediaSubsession
// ---------------------------------
class BaseServerMediaSubsession
{
 public:
 BaseServerMediaSubsession(StreamReplicator* replicator): m_replicator(replicator) {};
	
 public:
  static FramedSource* createSource(UsageEnvironment& env, FramedSource * videoES, int format);
  static RTPSink* createSink(UsageEnvironment& env, Groupsock * rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, int format);
  char const* getAuxLine(VideoDeviceSource* source,unsigned char rtpPayloadType);
		
 protected:
  StreamReplicator* m_replicator;
};

// -----------------------------------------
//    ServerMediaSubsession for Multicast
// -----------------------------------------
class MulticastServerMediaSubsession : public PassiveServerMediaSubsession , public BaseServerMediaSubsession
{
 public:
  static MulticastServerMediaSubsession* createNew(UsageEnvironment& env
						   , struct in_addr destinationAddress
						   , Port rtpPortNum, Port rtcpPortNum
						   , int ttl
						   , unsigned char rtpPayloadType
						   , StreamReplicator* replicator
						   , int format);
		
 protected:
 MulticastServerMediaSubsession(StreamReplicator* replicator, RTPSink* rtpSink, RTCPInstance* rtcpInstance) 
   : PassiveServerMediaSubsession(*rtpSink, rtcpInstance), BaseServerMediaSubsession(replicator), m_rtpSink(rtpSink) {};			

  virtual char const* sdpLines() ;
  virtual char const* getAuxSDPLine(RTPSink* rtpSink,FramedSource* inputSource);
		
 protected:
  RTPSink* m_rtpSink;
  std::string m_SDPLines;
};

// -----------------------------------------
//    ServerMediaSubsession for Unicast
// -----------------------------------------
class UnicastServerMediaSubsession : public OnDemandServerMediaSubsession , public BaseServerMediaSubsession
{
 public:
  static UnicastServerMediaSubsession* createNew(UsageEnvironment& env, StreamReplicator* replicator, int format);
		
 protected:
 UnicastServerMediaSubsession(UsageEnvironment& env, StreamReplicator* replicator, int format) 
   : OnDemandServerMediaSubsession(env, False), BaseServerMediaSubsession(replicator), m_format(format) {};
			
  virtual FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate);
  virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,  unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource);
  virtual char const* getAuxSDPLine(RTPSink* rtpSink,FramedSource* inputSource);
					
 protected:
  int m_format;
};

#endif
