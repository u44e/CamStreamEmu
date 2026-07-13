# Changelog

## 0.1.1
- Byte-exact SPS/PPS: the reproduced H.264 stream now emits the exact parameter
  sets captured in the profile (not the encoder's), over both multicast and RTSP.
- Fix: RTP-JPEG never actually streamed (rtpjpegpay rejected jpegenc's output);
  forcing I420 makes the SOF valid. Verified end-to-end.
- tests/verify.sh now covers all 7 delivery paths (adds RTP-JPEG).

## 0.1.0
- Initial release.
