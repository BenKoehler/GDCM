/*=========================================================================

  Program: GDCM (Grass Root DICOM). A DICOM library
  Module:  $URL$

  Copyright (c) 2006-2008 Mathieu Malaterre
  All rights reserved.
  See Copyright.txt or http://gdcm.sourceforge.net/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "gdcmJPEG2000Codec.h"
#include "gdcmTransferSyntax.h"
#include "gdcmTrace.h"
#include "gdcmDataElement.h"
#include "gdcmSequenceOfFragments.h"

#include "gdcm_openjpeg.h"

namespace gdcm
{

/**
sample error callback expecting a FILE* client object
*/
void error_callback(const char *msg, void *) {
  gdcmErrorMacro( "Error in gdcmopenjpeg" << msg );
}
/**
sample warning callback expecting a FILE* client object
*/
void warning_callback(const char *msg, void *) {
  gdcmWarningMacro( "Warning in gdcmopenjpeg" << msg );
}
/**
sample debug callback expecting no client object
*/
void info_callback(const char *msg, void *) {
  gdcmDebugMacro( "Info in gdcmopenjpeg" << msg );
}

#define J2K_CFMT 0
#define JP2_CFMT 1
#define JPT_CFMT 2
#define MJ2_CFMT 3
#define PXM_DFMT 0
#define PGX_DFMT 1
#define BMP_DFMT 2
#define YUV_DFMT 3

/*
 * Divide an integer by a power of 2 and round upwards.
 *
 * a divided by 2^b
 */
inline int int_ceildivpow2(int a, int b) {
  return (a + (1 << b) - 1) >> b;
}

class JPEG2000Internals
{
public:
};

JPEG2000Codec::JPEG2000Codec()
{
  Internals = new JPEG2000Internals;
  NumberOfDimensions = 0;
}

JPEG2000Codec::~JPEG2000Codec()
{
  delete Internals;
}

void JPEG2000Codec::SetNumberOfDimensions(unsigned int dim)
{
  NumberOfDimensions = dim;
}

bool JPEG2000Codec::CanDecode(TransferSyntax const &ts) const
{
  return ts == TransferSyntax::JPEG2000Lossless 
    || ts == TransferSyntax::JPEG2000;
}

bool JPEG2000Codec::CanCode(TransferSyntax const &ts) const
{
  return ts == TransferSyntax::JPEG2000Lossless 
    || ts == TransferSyntax::JPEG2000;
}

/*
A.4.4 JPEG 2000 image compression

  If the object allows multi-frame images in the pixel data field, then for these JPEG 2000 Part 1 Transfer
  Syntaxes, each frame shall be encoded separately. Each fragment shall contain encoded data from a
  single frame.
  Note: That is, the processes defined in ISO/IEC 15444-1 shall be applied on a per-frame basis. The proposal
  for encapsulation of multiple frames in a non-DICOM manner in so-called �Motion-JPEG� or �M-JPEG�
  defined in 15444-3 is not used.
*/
bool JPEG2000Codec::Decode(DataElement const &in, DataElement &out)
{
  if( NumberOfDimensions == 2 )
    {
    const SequenceOfFragments *sf = in.GetSequenceOfFragments();
    assert( sf );
    std::stringstream is;
    unsigned long totalLen = sf->ComputeByteLength();
    char *buffer = new char[totalLen];
    sf->GetBuffer(buffer, totalLen);
    is.write(buffer, totalLen);
    delete[] buffer;
    std::stringstream os;
    bool r = Decode(is, os);
    assert( r );
    out = in;
    std::string str = os.str();
    out.SetByteValue( &str[0], str.size() );
    //memcpy(buffer, os.str().c_str(), len);
    return r;
    }
  else if ( NumberOfDimensions == 3 )
    {
    /* I cannot figure out how to use openjpeg to support multiframes
     * as encoded in DICOM
     * MM: Hack. If we are lucky enough the number of encapsulated fragments actually match
     * the number of Z frames.
     * MM: hopefully this is the standard so people are following it ...
     */
    //#ifdef SUPPORT_MULTIFRAMESJ2K_ONLY
    const SequenceOfFragments *sf = in.GetSequenceOfFragments();
    assert( sf );
    std::stringstream os;
    for(unsigned int i = 0; i < sf->GetNumberOfFragments(); ++i)
      {
      std::stringstream is;
      const Fragment &frag = sf->GetFragment(i);
      if( frag.IsEmpty() ) return false;
      const ByteValue *bv = frag.GetByteValue();
      assert( bv );
      char *mybuffer = new char[bv->GetLength()];
      bv->GetBuffer(mybuffer, bv->GetLength());
      is.write(mybuffer, bv->GetLength());
      delete[] mybuffer;
      bool r = Decode(is, os);
      assert( r == true );
      }
    std::string str = os.str();
    out.SetByteValue( &str[0], str.size() );

    return true;
    }
  // else
  return false;
}

bool JPEG2000Codec::Decode(std::istream &is, std::ostream &os)
{
  opj_dparameters_t parameters;  /* decompression parameters */
  opj_event_mgr_t event_mgr;    /* event manager */
  opj_image_t *image;
  opj_dinfo_t* dinfo;  /* handle to a decompressor */
  opj_cio_t *cio;
  // FIXME: Do some stupid work:
  is.seekg( 0, std::ios::end);
  std::streampos buf_size = is.tellg();
  char *dummy_buffer = new char[buf_size];
  is.seekg(0, std::ios::beg);
  is.read( dummy_buffer, buf_size);
  unsigned char *src = (unsigned char*)dummy_buffer;
  int file_length = buf_size;

  /* configure the event callbacks (not required) */
  memset(&event_mgr, 0, sizeof(opj_event_mgr_t));
  event_mgr.error_handler = error_callback;
  event_mgr.warning_handler = warning_callback;
  event_mgr.info_handler = info_callback;

  /* set decoding parameters to default values */
  opj_set_default_decoder_parameters(&parameters);

  // default blindly copied
  parameters.cp_layer=0;
  parameters.cp_reduce=0;
  //   parameters.decod_format=-1;
  //   parameters.cod_format=-1;

  const char jp2magic[] = "\x00\x00\x00\x0C\x6A\x50\x20\x20\x0D\x0A\x87\x0A";
  if( memcmp( src, jp2magic, sizeof(jp2magic) ) == 0 )
    {
    /* JPEG-2000 compressed image data */
    // gdcmData/ELSCINT1_JP2vsJ2K.dcm
    gdcmWarningMacro( "J2K start like JPEG-2000 compressed image data instead of codestream" );
    parameters.decod_format = JP2_CFMT;
    assert(parameters.decod_format == JP2_CFMT);
    }
  else
    {
    /* JPEG-2000 codestream */
    parameters.decod_format = J2K_CFMT;
    assert(parameters.decod_format == J2K_CFMT);
    }
  parameters.cod_format = PGX_DFMT;
  assert(parameters.cod_format == PGX_DFMT);

  /* get a decoder handle */
  switch(parameters.decod_format )
    {
  case J2K_CFMT:
    dinfo = opj_create_decompress(CODEC_J2K);
    break;
  case JP2_CFMT:
    dinfo = opj_create_decompress(CODEC_JP2);
    break;
  default:
    abort();
    }

  /* catch events using our callbacks and give a local context */
  opj_set_event_mgr((opj_common_ptr)dinfo, &event_mgr, NULL);      

  /* setup the decoder decoding parameters using user parameters */
  opj_setup_decoder(dinfo, &parameters);

  /* open a byte stream */
  cio = opj_cio_open((opj_common_ptr)dinfo, src, file_length);

  /* decode the stream and fill the image structure */
  image = opj_decode(dinfo, cio);
  if(!image) {
    opj_destroy_decompress(dinfo);
    opj_cio_close(cio);
    return 1;
  }

  /* close the byte stream */
  opj_cio_close(cio);

  //raw = (char*)src;
  // Copy buffer
    char *raw = NULL;
  //assert(image->prec % 8 == 0);
  unsigned long len = Dimensions[0]*Dimensions[1]*Dimensions[2] * (PF.GetBitsAllocated() / 8) * image->numcomps;
  raw = new char[len];
  for (int compno = 0; compno < image->numcomps; compno++)
    {
    opj_image_comp_t *comp = &image->comps[compno];

    int w = image->comps[compno].w;
    int wr = int_ceildivpow2(image->comps[compno].w, image->comps[compno].factor);

    //int h = image.comps[compno].h;
    int hr = int_ceildivpow2(image->comps[compno].h, image->comps[compno].factor);
      assert(  wr * hr * 1 * image->numcomps == len );

    if (comp->prec <= 8)
      {
      assert( comp->prec == 8 && comp->prec == PF.GetBitsAllocated() );
      uint8_t *data8 = (uint8_t*)raw + compno;
      for (int i = 0; i < wr * hr; i++) 
        {
        int v = image->comps[compno].data[i / wr * w + i % wr];
        *data8 = (uint8_t)v;
        data8 += image->numcomps;
        }
      }
    else if (comp->prec <= 16)
      {
      assert( comp->prec == 16 && comp->prec == PF.GetBitsAllocated());
      uint16_t *data16 = (uint16_t*)raw + compno;
      for (int i = 0; i < wr * hr; i++) 
        {
        int v = image->comps[compno].data[i / wr * w + i % wr];
        *data16 = (uint16_t)v;
        data16 += image->numcomps;
        }
      }
    else
      {
      assert( comp->prec == 32 && comp->prec == PF.GetBitsAllocated());
      uint32_t *data32 = (uint32_t*)raw + compno;
      for (int i = 0; i < wr * hr; i++) 
        {
        int v = image->comps[compno].data[i / wr * w + i % wr];
        *data32 = (uint32_t)v;
        data32 += image->numcomps;
        }
      }
    //free(image.comps[compno].data);
    }
    os.write(raw, len );
    delete[] raw;
  /* free the memory containing the code-stream */
  delete[] src;  //FIXME



  /* free remaining structures */
  if(dinfo) {
    opj_destroy_decompress(dinfo);
  }

  /* free image data structure */
  opj_image_destroy(image);

  return true;
}

template<typename T>
void rawtoimage_fill(T *inputbuffer, int w, int h, int numcomps, opj_image_t *image)
{
  T *p = inputbuffer;
  for (int i = 0; i < w * h; i++)
    {
    for(int compno = 0; compno < numcomps; compno++)
      {
      /* compno : 0 = GREY, (0, 1, 2) = (R, G, B) */
      image->comps[compno].data[i] = *p;
      ++p;
      }
    }
}

opj_image_t* rawtoimage(char *inputbuffer, opj_cparameters_t *parameters,
  int fragment_size, int image_width, int image_height, int sample_pixel,
  int bitsallocated, int sign, int quality)
{
  (void)quality;
  int w, h;
  int numcomps;
  OPJ_COLOR_SPACE color_space;
  opj_image_cmptparm_t cmptparm[3]; /* maximum of 3 components */
  opj_image_t * image = NULL;

  assert( sample_pixel == 1 || sample_pixel == 3 );
  if( sample_pixel == 1 )
    {
    numcomps = 1;
    color_space = CLRSPC_GRAY;
    }
  else // sample_pixel == 3
    {
    numcomps = 3;
    color_space = CLRSPC_SRGB;
    }
  assert( bitsallocated % 8 == 0 );
  assert( fragment_size == image_height * image_width * numcomps * (bitsallocated/8) );
  int subsampling_dx = parameters->subsampling_dx;
  int subsampling_dy = parameters->subsampling_dy;

  // FIXME
  w = image_width;
  h = image_height;

  /* initialize image components */
  memset(&cmptparm[0], 0, 3 * sizeof(opj_image_cmptparm_t));
  //assert( bitsallocated == 8 );
  for(int i = 0; i < numcomps; i++) {
    cmptparm[i].prec = bitsallocated;
    cmptparm[i].bpp = bitsallocated;
    cmptparm[i].sgnd = sign;
    cmptparm[i].dx = subsampling_dx;
    cmptparm[i].dy = subsampling_dy;
    cmptparm[i].w = w;
    cmptparm[i].h = h;
  }

  /* create the image */
  image = opj_image_create(numcomps, &cmptparm[0], color_space);
  if(!image) {
    return NULL;
  }
  /* set image offset and reference grid */
  image->x0 = parameters->image_offset_x0;
  image->y0 = parameters->image_offset_y0;
  image->x1 = parameters->image_offset_x0 + (w - 1) * subsampling_dx + 1;
  image->y1 = parameters->image_offset_y0 + (h - 1) * subsampling_dy + 1;

  /* set image data */

  //assert( fragment_size == numcomps*w*h*(bitsallocated/8) );
  if (bitsallocated <= 8)
    {
    if( sign )
      {
      rawtoimage_fill<int8_t>((int8_t*)inputbuffer,w,h,numcomps,image);
      }
    else
      {
      rawtoimage_fill<uint8_t>((uint8_t*)inputbuffer,w,h,numcomps,image);
      }
    }
  else if (bitsallocated <= 16)
    {
    if( sign )
      {
      rawtoimage_fill<int16_t>((int16_t*)inputbuffer,w,h,numcomps,image);
      }
    else
      {
      rawtoimage_fill<uint16_t>((uint16_t*)inputbuffer,w,h,numcomps,image);
      }
    }
  else if (bitsallocated <= 32)
    {
    if( sign )
      {
      rawtoimage_fill<int32_t>((int32_t*)inputbuffer,w,h,numcomps,image);
      }
    else
      {
      rawtoimage_fill<uint32_t>((uint32_t*)inputbuffer,w,h,numcomps,image);
      }
    }
  else
    {
    abort();
    }

  return image;
}

  // Compress into JPEG
bool JPEG2000Codec::Code(DataElement const &in, DataElement &out)
{
  out = in;
  //
  // Create a Sequence Of Fragments:
  SmartPointer<SequenceOfFragments> sq = new SequenceOfFragments;
  const Tag itemStart(0xfffe, 0xe000);
  sq->GetTable().SetTag( itemStart );

  const unsigned int *dims = this->GetDimensions();

  const ByteValue *bv = in.GetByteValue();
  const char *input = bv->GetPointer();
  unsigned long len = bv->GetLength();
  unsigned long image_len = len / dims[2];
  size_t inputlength = image_len;

  for(unsigned int dim = 0; dim < dims[2]; ++dim)
    {

    std::ostringstream os;
    std::ostream *fp = &os;
    const char *inputdata = input + dim * image_len; //bv->GetPointer();
    //size_t inputlength = bv->GetLength();
    int image_width = dims[0];
    int image_height = dims[1];
    int numZ = 0; //dims[2];
    const PixelFormat &pf = this->GetPixelFormat();
    int sample_pixel = pf.GetSamplesPerPixel();
    int bitsallocated = pf.GetBitsAllocated();
    int sign = pf.GetPixelRepresentation();
    int quality = 100;

    //// input_buffer is ONE image
    //// fragment_size is the size of this image (fragment)
    (void)numZ;
    bool bSuccess;
    //bool delete_comment = true;
    opj_cparameters_t parameters;  /* compression parameters */
    opj_event_mgr_t event_mgr;    /* event manager */
    opj_image_t *image = NULL;
    //quality = 100;

    /*
    configure the event callbacks (not required)
    setting of each callback is optionnal
    */
    memset(&event_mgr, 0, sizeof(opj_event_mgr_t));
    event_mgr.error_handler = error_callback;
    event_mgr.warning_handler = warning_callback;
    event_mgr.info_handler = info_callback;

    /* set encoding parameters to default values */
    memset(&parameters, 0, sizeof(parameters));
    opj_set_default_encoder_parameters(&parameters);

    /* if no rate entered, lossless by default */
    parameters.tcp_rates[0] = 0;
    parameters.tcp_numlayers = 1;
    parameters.cp_disto_alloc = 1;

    if(parameters.cp_comment == NULL) {
      const char comment[] = "Created by GDCM/OpenJPEG version 1.0";
      parameters.cp_comment = (char*)malloc(strlen(comment) + 1);
      strcpy(parameters.cp_comment, comment);
      /* no need to delete parameters.cp_comment on exit */
      //delete_comment = false;
    }


    /* decode the source image */
    /* ----------------------- */

    image = rawtoimage((char*)inputdata, &parameters, 
      static_cast<int>( inputlength ), 
      image_width, image_height,
      sample_pixel, bitsallocated, sign, quality);
    if (!image) {
      return 1;
    }

    /* encode the destination image */
    /* ---------------------------- */
    parameters.cod_format = J2K_CFMT; /* J2K format output */
    int codestream_length;
    opj_cio_t *cio = NULL;
    //FILE *f = NULL;

    /* get a J2K compressor handle */
    opj_cinfo_t* cinfo = opj_create_compress(CODEC_J2K);

    /* catch events using our callbacks and give a local context */
    opj_set_event_mgr((opj_common_ptr)cinfo, &event_mgr, stderr);

    /* setup the encoder parameters using the current image and using user parameters */
    opj_setup_encoder(cinfo, &parameters, image);

    /* open a byte stream for writing */
    /* allocate memory for all tiles */
    cio = opj_cio_open((opj_common_ptr)cinfo, NULL, 0);

    /* encode the image */
    bSuccess = opj_encode(cinfo, cio, image, parameters.index);
    if (!bSuccess) {
      opj_cio_close(cio);
      fprintf(stderr, "failed to encode image\n");
      return 1;
    }
    codestream_length = cio_tell(cio);

    /* write the buffer to disk */
    //f = fopen(parameters.outfile, "wb");
    //if (!f) {
    //  fprintf(stderr, "failed to open %s for writing\n", parameters.outfile);
    //  return 1;
    //}
    //fwrite(cio->buffer, 1, codestream_length, f);
    //#define MDEBUG
#ifdef MDEBUG
    static int c = 0;
    std::ostringstream os;
    os << "/tmp/debug";
    os << c;
    c++;
    os << ".j2k";
    std::ofstream debug(os.str().c_str());
    debug.write((char*)(cio->buffer), codestream_length);
    debug.close();
#endif
    fp->write((char*)(cio->buffer), codestream_length);
    //fclose(f);

    /* close and free the byte stream */
    opj_cio_close(cio);

    /* free remaining compression structures */
    opj_destroy_compress(cinfo);


    /* free user parameters structure */
    //if(delete_comment) {
    if(parameters.cp_comment) free(parameters.cp_comment);
    //}
    if(parameters.cp_matrice) free(parameters.cp_matrice);

    /* free image data */
    opj_image_destroy(image);



    std::string str = os.str();
    assert( str.size() );
    Fragment frag;
    frag.SetTag( itemStart );
    frag.SetByteValue( &str[0], str.size() );
    sq->AddFragment( frag );
    }

  //unsigned int nfrags = sq->GetNumberOfFragments();
  assert( sq->GetNumberOfFragments() == dims[2] );
  out.SetValue( *sq );

  return true;
}

} // end namespace gdcm
