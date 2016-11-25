/* v2scan - VIVID scanning tool
 *
 */

/* includes {{{ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "getopt/getopt.h"
#include "tiff/tiffio.h"
#include "vividIIsdk/vividIIsdk.h"
//}}}

/* constants {{{ */
const char* appname="v2scan";
const char* version="1.0";
//}}}

/* options {{{ */
int verbose=0;
int passiveaf=0;
int activeaf=0;
int activeafae=0;
char *output=0;
char *format="TIFF";
int distance=-1;
int laserpower=-1;
int gain=-1;
int rmode=-1;
int threshold=-1;
int autoread=-1;
int color=-1;
int start=-1;
int count=1;
int dynrangeexp=0;
int subsampling=-1;
int noise=-1;
int fillhole=-1;
int dark=-1;
//}}}

/* globals {{{ */
VVDII_CameraMode cammode;
VVDII_CameraData *camdata=NULL;
VVD_Object *object;
VVDII_ImportPara imp_para;
//}}}

/* forward declarations {{{ */
void print_usage();
void handle_error(char *message);
void get_cammode();
void set_cammode();
void cmd_status();
void cmd_scan();
void cmd_image();
void perform_release();

//}}}

/* print usage {{{ */
void
print_usage()
{
	printf("Usage: %s [options] command\n",appname);
	printf("\n");
	printf("Options:\n");
	printf("  -v,--verbose              be verbose\n");
	printf("  -h,--help                 display usage\n");
	printf("  -V,--version              show version info\n");
	printf("  -p,--passiveaf            perform passive AF before scan\n");
	printf("  -a,--activeaf             perform active AF before scan\n");
	printf("  -e,--activeafae           perform active AF/AE before scan (VIVID910)\n");
	printf("  -x,--dynrangeexp          scan in dynamic range expansion mode (VIVID910)\n");
	printf("  -r,--rotate N START       rotate turntable (only for scan, uses stage.exe)\n");
	printf("                            scan N times, starting from START angle\n");
	printf("  -o,--output FILE          use FILE as output (for scan and image)\n");
	printf("  -f,--format FORMAT        use FORMAT for output (after extension otherwise))\n");
	printf("  -d,--distance DIST        parameter: distance in mm (500-2500)\n");
	printf("  -l,--laserpower POWER     parameter: laser power (0-255, 0:laser off)\n");
	printf("  -g,--gain GAIN            parameter: gain (0-7)\n");
	printf("  -m,--mode MODE            parameter: release mode (0-7)\n");
	printf("                            0:FINE&COLOR 1:FAST&COLOR 2:COLOR(8bit) 3:COLOR(10bit)\n");
	printf("                            4:MONITOR(8bit) 5:R(8bit) 6:G(8bit) 7:B(8bit)\n");
	printf("  -t,--threshold THRE       parameter: threshold (0-1023, 65535:auto)\n");
	printf("  -u,--autoread 0|1         parameter: autoread (0:on/pitch with color , 1:off/only pitch)\n");
	printf("  -c,--color COLOR          parameter: color (0-10, 10:auto)\n");
	printf("  -b,--subsampling RATE     filter: subsampling rate (1-4, 1:1/1, 2:1/4, 3:1/9, 4:1/16)\n");
	printf("  -n,--noise QUAL           filter: noise filter (0-3)\n");
	printf("                            0:no, 1:noise, 2:hq (VIVID910), 3:noise & hq (VIVID910)\n");
	printf("  -i,--fillhole             filter: fill holes\n");
	printf("  -k,--dark                 filter: color dark correction\n");
	printf("\n");
	printf("Commands:\n");
	printf("  status                    show scanner status\n");
	printf("  scan                      perform scan\n");
	printf("  image                     get image from scanner\n");
	printf("\n");
	exit(1);
}// print_usage() }}}

/* finish scsi and exit {{{ */
void
finish_and_exit()
{
	if(verbose) printf("Closing SCSI device...\n");
	VividIISCSIFinish();
	if(camdata) VividIIFreeCameraData(&camdata);
	exit(1);
} // finish_and_exit() }}}

/* error handling {{{ */
void
handle_error(char *message)
{
	int errorcode;
	errorcode=VividGetErrorStatus();
	printf("%s Error %d (", message, errorcode);
	switch(errorcode)
	{
		case SERR_BUSY: printf("timeout error"); break;
		case SERR_WRITE: printf("scsi write error"); break;
		case SERR_READ: printf("scsi read error"); break;
		case SERR_BLOCK: printf("block error"); break;
		case SERR_POWERON: printf("power on reset error"); break;
		case SERR_HARD: printf("hardware error"); break;
		case SERR_PCFORMAT: printf("pccard format error"); break;
		case SERR_NONATA: printf("non supported pccard"); break;
		case SERR_NOPCCARD: printf("no pccard present"); break;
		case SERR_PARITY: printf("scsi parity error"); break;
		case SERR_READY: printf("ready command error"); break;
		case SERR_OUTOFDIST: printf("out of distance"); break;
		case SERR_HDDRESET: printf("unit reset or hdd changed"); break;
		case SERR_NOTFOUND: printf("vivid not found"); break;
		case SERR_ANY: printf("any error"); break;
		case SERR_MEMORY: printf("scsi memory error"); break;
		case SERR_ARGUMENT: printf("scsi argument error"); break;
		case VERROR_MEM_ALLOC: printf("memory allocation error"); break;
		case VERROR_OPEN_FILE: printf("file open error"); break;
		case VERROR_READ_FILE: printf("file read error"); break;
		case VERROR_NOT_PRODUCT: printf("not a vivid file"); break;
		case VERROR_INVALID_MAGIC: printf("invalid magic number"); break;
		case VERROR_UNKNOWN_TYPE: printf("unknown type"); break;
		case VERROR_INVALID_ARGS: printf("invalid argument"); break;
		case VERROR_WRITE_FILE: printf("file write error"); break;
		case VERROR_NO_IMAGE: printf("has no image"); break;
		case VERROR_MULT_DATA: printf("not a single data file"); break;
		case VERROR_SINGLE_DATA: printf("not a multi data file"); break;
		default: printf("unknown error"); break;
	}
	printf(")\n");
}// error_handling() }}}

/* perform release {{{ */
void
perform_release()
{
	int errorcode;

	/* dynamic range expansion */
	if(dynrangeexp)
	{
		if(verbose) printf("Using dynamic range expansion...\n");
		errorcode=VividIISCSIScanRead910(&camdata,cammode.distance,
			cammode.laserPower,cammode.gain,1);
		if(errorcode==VVD_ILLEGAL)
		{
			printf("Release Error (No Vivid 910)\n");
			finish_and_exit();
		}
		if(errorcode==VVD_FALSE) handle_error("Release");
	}

	/* standard release mode */
	else
	{
		if(VividIISCSIRelease()==VVD_FALSE) handle_error("Release");
		if(VividIISCSIReadPitch(&camdata)==VVD_FALSE) handle_error("Read Pitch");
		if(VividIISCSIReadColor(&camdata,cammode.r_mode)==VVD_FALSE) handle_error("Read Color");
	}
}// perform_release() }}}

/* get current camera mode {{{ */
void
get_cammode()
{
	/* init SCSI device and read camera mode */
	if(verbose) printf("Initializing SCSI device...\n");
	if(VividIISCSIInitialize()==VVD_FALSE)
	{
		handle_error("SCSI Initialize");
		exit(1);
	}
	if(verbose) printf("Reading camera parameters...\n");
	if(VividIISCSIReadParameter(&cammode)==VVD_FALSE)
	{
		handle_error("Read Camera Mode");
		finish_and_exit();
	}
}// get_cammode() }}}

/* status command {{{ */
void
cmd_status()
{
	get_cammode();
	set_cammode();
	printf("VividII Camera Status:\n");
	printf("----------------------\n");
	printf("Distance:         %dmm\n",cammode.distance);
    printf("Laser Power:      %d\n",cammode.laserPower);
    printf("Gain:             %d\n",cammode.gain);
    printf("RMode:            %d\n",cammode.r_mode);
	printf("Threshold:        %d\n",cammode.threshold);
    printf("Auto Read:        %d\n",cammode.autoRead);
    printf("Color correction: %d\n",cammode.color);
	finish_and_exit();
}// cmd_status() }}}

/* scan command {{{ */
void
cmd_scan()
{
	FILE *out;
	unsigned long *buffer;
	int x,y;
	char filename[256];
	char stagecmd[256];
	int i;
	camdata=(VVDII_CameraData *)malloc(sizeof(VVDII_CameraData));
	if(!output) output="image.hdr";
	sprintf((char *)&filename,output);

	/* get/set parameters */
	get_cammode();
	set_cammode();

	/* loop for rotating */
	for(i=1;i<=count;i++,start+=360/count)
	{
		/* execute stage.exe */
		if(count>1)
		{
			sprintf((char *)&filename,"%s%d.%s",output,i,format);
			sprintf((char *)&stagecmd,"stage.exe -r %d",start);
			if(verbose) printf("Rotating: %s...\n",&stagecmd);
			system((char *)&stagecmd);
		}

		perform_release();

		/* open file for writing */
		if(verbose) printf("Open %s for writing...\n",(char *)&filename);
		out=fopen((char *)&filename,"w");
		if(!out)
		{
			printf("Couldn't open %s for writing!\n",(char *)&filename);
			finish_and_exit();
		}

		/* header */
		if(verbose) printf("Writing header...\n");
		fprintf(out,"IBRraw.xdr\n");
		fprintf(out,"@@ImageDim = 3\n");
		fprintf(out,"@@ImageSize = 640 480\n");
		fprintf(out,"@@buffer-channels-0 = 3\n");
		fprintf(out,"@@buffer-primtype-0 = byte\n");
		fprintf(out,"@@buffer-type-0 = color\n");
		fprintf(out,"---end-of-header---\n");

		/* data */
		if(verbose) printf("Writing data...\n");
		buffer=camdata->data3d;
		for(y=0;y<480;y++)
		{
			if(verbose) printf(".");
			for(x=0;x<640;x++)
			{
				fprintf(out,"%d\n",*buffer++);
			}
		}
		if(verbose) printf("\n");
		fclose(out);
	}
	finish_and_exit();
}// cmd_scan() }}}

/* image command {{{ */
void
cmd_image()
{
	TIFF *tif;
	VVD_Image *image;
	char filename[256];
	char stagecmd[256];
	int i,j;
	int buflen;
	char *bufptr;
	char tmp;
	camdata=(VVDII_CameraData *)malloc(sizeof(VVDII_CameraData));
	image=(VVD_Image *)malloc(sizeof(VVD_Image));
	if(!output) output="image";
	sprintf((char *)&filename,"%s.%s",output,format);

	/* get/set parameters */
	get_cammode();
	set_cammode();

	/* loop for rotate */
	for(i=1;i<=count;i++,start+=360/count)
	{
		/* execute stage.exe */
		if(count>1)
		{
			sprintf((char *)&filename,"%s%d.%s",output,i,format);
			sprintf((char *)&stagecmd,"stage.exe -r %d",start);
			if(verbose) printf("Rotating: %s...\n",&stagecmd);
			system((char *)&stagecmd);
		}

		/* get color image */
		perform_release();
		if(VividIIPickupColorImage(camdata,&image)==VVD_FALSE)
			handle_error("Pickup Color Image");
		if(verbose) printf("ImageType: %d, Width: %d, Height: %d\n",
			image->attribute,image->width,image->height);

		/* write TIFF */
		if(!strcmp(format,"TIFF"))
		{
			if(verbose) printf("Open %s for writing (format %s)...\n",&filename,format);
			tif=TIFFOpen((char *)&filename,"w");
			if(!tif)
			{
				printf("Couldn't open %s for writing...\n",&filename);
				finish_and_exit();
			}
			TIFFSetField(tif,TIFFTAG_IMAGEWIDTH,image->width);
			TIFFSetField(tif,TIFFTAG_IMAGELENGTH,image->height);
			TIFFSetField(tif,TIFFTAG_SAMPLESPERPIXEL,4);
			TIFFSetField(tif,TIFFTAG_BITSPERSAMPLE,8);
			TIFFSetField(tif,TIFFTAG_ORIENTATION,ORIENTATION_TOPLEFT);
			TIFFSetField(tif,TIFFTAG_PLANARCONFIG,PLANARCONFIG_CONTIG);
			TIFFSetField(tif,TIFFTAG_PHOTOMETRIC,PHOTOMETRIC_RGB); 
			TIFFSetField(tif,TIFFTAG_COMPRESSION,COMPRESSION_LZW);
			buflen=image->width*image->height*sizeof(VVD_Pixel);
			bufptr=(char *)(image->pixels);
			for(j=0;j<buflen;j+=4)
			{
				*(bufptr)=*(bufptr+3);
				tmp=*(bufptr+1);
				*(bufptr+1)=*(bufptr+2);
				*(bufptr+2)=tmp;
				bufptr+=4;
			}
			if(verbose) printf("Writing strip (%d bytes)...\n",buflen);
			TIFFWriteEncodedStrip(tif,0,image->pixels,buflen);
			TIFFClose(tif);
			printf("Wrote %s ...\n",(char *)&filename);
		}
		else
		{
			printf("Unknown format! Supported formats: TIFF\n");
		}
	}
	free(image);
	finish_and_exit();
}// cmd_image() }}}

/* sets camera mode {{{ */
void
set_cammode()
{
	/* passive AF */
	if(passiveaf)
	{
		if(verbose) printf("Performing Passive AF...\n");
		if(VividIISCSIPassiveAF(&cammode)==VVD_FALSE)
		{	
			handle_error("Passive AF");
			finish_and_exit();
		}
	}
	/* active AF */
	if(activeaf)
	{
		if(verbose) printf("Performing Active AF...\n");
		if(VividIISCSIActiveAF(&cammode)==VVD_FALSE)
		{	
			handle_error("Active AF");
			finish_and_exit();
		}
	}
	/* active AF/AE */
	if(activeafae)
	{
		if(verbose) printf("Performing Active AF/AE...\n");
		if(VividIISCSIActiveAFAE_910(&cammode)==VVD_FALSE)
		{	
			handle_error("Active AF/AE");
			finish_and_exit();
		}
	}

	/* camera mode parameters... */
	if(distance>=0)
	{
		if(verbose) printf("Setting distance to %d...\n",distance);
		cammode.distance=distance;
	}
	if(gain>=0)
	{
		if(verbose) printf("Setting gain to %d...\n",gain);
		cammode.gain=gain;
	}
	if(rmode>=0)
	{
		if(verbose) printf("Setting rmode to %d...\n",rmode);
		cammode.r_mode=rmode;
	}
	if(threshold>=0)
	{
		if(verbose) printf("Setting threshold to %d...\n",threshold);
		cammode.threshold=threshold;
	}
	if(autoread>=0)
	{
		if(verbose) printf("Setting autoread to %d...\n",autoread);
		cammode.autoRead=autoread;
	}
	if(color>=0)
	{
		if(verbose) printf("Setting color to %d...\n",color);
		cammode.color=color;
	}
	if(laserpower>=0)
	{
		if(verbose) printf("Setting laserpower to %d...\n",laserpower);
		cammode.laserPower=laserpower;
	}

	/* import filters (conversion) */
	if(fillhole>=0)
	{
		if(verbose) printf("Using fillhole filter...\n");
		imp_para.eFillHole=1;
	}
	if(dark>=0)
	{
		if(verbose) printf("Using color dark correction filter...\n");
		imp_para.bDark=1;
	}
	if(subsampling>=0)
	{
		if(verbose) printf("Using subsampling filter...\n");
		imp_para.eReduce=subsampling;
	}
	if(noise>=0)
	{
		if(verbose) printf("Using noise filter...\n");
		imp_para.eFilter=noise;
	}	

	/* write parameters back */
	if(verbose) printf("Writing parameters...\n");
	if(VividIISCSIWriteParameter(&cammode)==VVD_FALSE)
	{
		handle_error("Write Camera Mode");
		finish_and_exit();
	}
}// set_cammode() }}}

/* main {{{ */
int
main(int argc,char **argv)
{
	int optchar;
	char *command;

	/* parse options {{{ */
	while(1)
	{
		static struct option long_options[]=
		{
			{"verbose",no_argument,&verbose,1},
			{"help",no_argument,0,'h'},
			{"version",no_argument,0,'V'},
			{"passiveaf",no_argument,&passiveaf,1},
			{"activeaf",no_argument,&activeaf,1},
			{"activeafae",no_argument,&activeafae,1},
			{"rotate",required_argument,0,'r'},
			{"output",required_argument,0,'o'},
			{"format",required_argument,0,'f'},
			{"distance",required_argument,0,'d'},
			{"laserpower",required_argument,0,'l'},
			{"gain",required_argument,0,'g'},
			{"mode",required_argument,0,'m'},
			{"threshold",required_argument,0,'t'},
			{"autoread",required_argument,0,'u'},
			{"color",required_argument,0,'c'},
			{"subsampling",required_argument,0,'b'},
			{"noise",required_argument,0,'n'},
			{"fillhole",no_argument,&fillhole,1},
			{"dark",no_argument,&dark,1},
			{0,0,0,0}
		};
		int option_index=0;
		
		/* get next option */
		optchar=getopt_long(argc,argv,"vhVpaexikr:o:f:d:l:g:m:t:u:c:b:n:",long_options,&option_index);

		/* no more options */
		if(optchar==-1)
			break;

		switch(optchar)
		{
			/* found long option */
			case 0:
				if(long_options[option_index].flag!=0)
					break;
				break;
			
			/* bool options */
			case 'v':
				verbose=1;
				break;
			case 'p':
				passiveaf=1;
				break;
			case 'a':
				activeaf=1;
				break;
			case 'e':
				activeafae=1;
				break;
			case 'x':
				dynrangeexp=1;
				break;
			case 'i':
				fillhole=1;
				break;
			case 'k':
				dark=1;
				break;
			
			/* string options */
			case 'o':
				output=optarg;
				break;
			case 'f':
				format=optarg;
				break;

			/* numeric parameters */
			case 'd':
				distance=atoi(optarg);
				if(distance<500||distance>2500)
					{ printf("Error: distance has to be between 500-2500\n"); exit(0); }
				break;
			case 'l':
				laserpower=atoi(optarg);
				if(laserpower<0||laserpower>255)
					{ printf("Error: laserpower has to be between 0-255 (0:laser off)\n"); exit(0); }
				break;
			case 'g':
				gain=atoi(optarg);
				if(gain<0||gain>7)
					{ printf("Error: gain has to be between 0-7\n"); exit(0); }
				break;
			case 'm':
				rmode=atoi(optarg);
				if(rmode<0||rmode>7)
					{ printf("Error: rmode has to be between 0-7:\n");
						printf(" 0:FINE&COLOR 1:FAST&COLOR 2:COLOR(8bit) 3:COLOR(10bit)\n");
						printf(" 4:MONITOR(8bit) 5:R(8bit) 6:G(8bit) 7:B(8bit)\n"); exit(0); }
				break;
			case 't':
				threshold=atoi(optarg);
				if((threshold<0||threshold>1023)&&threshold!=65535)
					{ printf("Error: threshold has to be between 0-1023 (65535:auto)\n"); exit(0); }
				break;
			case 'u':
				autoread=atoi(optarg);
				if(autoread<0||autoread>1)
					{ printf("Error: autoread has to be 0 or 1:");
					  printf(" 0:on/pitch with color), 1:off/only pitch\n"); exit(0); }
				break;
			case 'c':
				color=atoi(optarg);
				if(color<0||color>10)
					{ printf("Error: color has to be between 0-10 (10:auto)\n"); exit(0); }
				break;
			case 'b':
				subsampling=atoi(optarg);
				if(subsampling<1||subsampling>5)
					{ printf("Error: subsampling has to be between 1-4:");
					  printf(" 1:1/1, 2:1/4, 3:1/9, 4:1/16\n"); exit(0); }
				break;
			case 'n':
				noise=atoi(optarg);
				if(noise<0||noise>3)
					{ printf("Error: noise has to be between 0-3:");
					  printf(" 0:no, 1:noise, 2:hq (VIVID910), 3:noise & hq (VIVID910) \n"); exit(0); }
				break;
			case 'V':
				printf("%s - version %s\n",appname,version);
				exit(0);

			/* rotate turntable option */
			case 'r':
				count=atoi(optarg);
				if(optind>=argc) { print_usage(); }
				start=atoi(argv[optind++]);
				break;
			
			/* unrecognized option */
			case '?':
				print_usage();
				break;
				
			/* ...should never come here */
			default:
				abort();
		}// switch(optchar)
	}// while(1)

	/* get command string */
	if(optind<argc) command=argv[optind];
	else print_usage();
	//}}}
	
	/* perform command */
	if(!strcmp(command,"status")) cmd_status();
	else if(!strcmp(command,"scan")) cmd_scan();
	else if(!strcmp(command,"image")) cmd_image();
	/* unknown command */
	else print_usage();
	//printf("%s\n",format);

	exit(0);
}// main() }}}

// vim:fdm=marker:
