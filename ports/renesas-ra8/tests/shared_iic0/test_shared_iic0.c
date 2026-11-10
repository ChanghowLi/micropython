#include "stubs.h"
#include "i2c.h"
#include <stdio.h>
static iic_master_instance_ctrl_t ctrl;
static i2c_master_cfg_t active_cfg;
static const i2c_master_cfg_t cfg;
static unsigned opens, closes, writes, reads;
static int open_error, close_error, write_error, read_error;
static bool last_restart;
static i2c_t *probe_bus;
static bool probe_interleave;
static fsp_err_t test_open(void *c,const i2c_master_cfg_t *p) { (void)c;opens++;if(open_error)return open_error;active_cfg=*p;return 0; }
static fsp_err_t test_close(void *c) { (void)c;closes++;return close_error; }
static fsp_err_t complete(void *c,bool read,bool restart) {
 ((iic_master_instance_ctrl_t *)c)->restarted=restart;last_restart=restart;
 i2c_master_callback_args_t a={active_cfg.p_context,read?I2C_MASTER_EVENT_RX_COMPLETE:I2C_MASTER_EVENT_TX_COMPLETE};active_cfg.p_callback(&a);return 0;
}
static fsp_err_t test_write(void *c,uint8_t *d,uint32_t n,bool restart) {
 (void)d;(void)n;writes++;
 if(probe_interleave) { probe_interleave=false; uint8_t b=0; assert(IIC_Write(probe_bus,0x38,&b,1)==FSP_ERR_TIMEOUT); }
 if(write_error)return write_error;
 return complete(c,false,restart);
}
static fsp_err_t test_read(void *c,uint8_t *d,uint32_t n,bool restart) { reads++;assert(!last_restart); if(read_error)return read_error; if(n)d[0]=0x56;return complete(c,true,restart); }
static fsp_err_t test_abort(void *c) { ((iic_master_instance_ctrl_t *)c)->restarted=false;return 0; }
static fsp_err_t test_address(void *c,uint32_t a,int mode) { (void)mode;((iic_master_instance_ctrl_t *)c)->slave=a;return 0; }
const i2c_master_api_t g_i2c_master_on_iic={test_open,test_close,test_read,test_write,test_abort,test_address};
const i2c_master_api_t g_i2c_master_on_sci_b={test_open,test_close,test_read,test_write,test_abort,test_address};
const i2c_master_instance_t g_i2c_master0={&ctrl,&cfg,&g_i2c_master_on_iic};
test_ioport_t g_ioport;
void test_camera_board(void);
int main(void) {
 i2c_t *lcd=NULL,*cam=NULL;
 i2c_t python_bus={.instance=&g_i2c_master0};
 assert(IIC_BoardAcquire(NULL)==FSP_ERR_INVALID_ARGUMENT);
 assert(IIC_BoardAcquire(&lcd)==0);
 assert(IIC_BoardAcquire(&cam)==0 && lcd==cam && opens==1);
 assert(IIC_Claim(&python_bus)==FSP_ERR_IN_USE);
 assert(IIC_Init(&python_bus)==FSP_ERR_IN_USE);
 assert(IIC_DeInit(lcd)==FSP_ERR_IN_USE);
 uint8_t value=0;
 probe_bus=lcd;probe_interleave=true;
 assert(IIC_ReadRegSCCB(cam,0x3c,0x300a,2,&value)==0 && value==0x56);
 assert(writes==1 && reads==1 && !last_restart);
 assert(IIC_ReadRegSCCB(cam,0x21,0x0a,1,&value)==0);
 assert(IIC_ReadRegSCCB(cam,0x21,0x100,1,&value)==FSP_ERR_INVALID_ARGUMENT);
 write_error=FSP_ERR_ABORTED;
 assert(IIC_ReadRegSCCB(cam,0x3c,0x300a,2,&value)==FSP_ERR_ABORTED);
 write_error=0;
 read_error=FSP_ERR_ABORTED;
 assert(IIC_ReadRegSCCB(cam,0x3c,0x300a,2,&value)==FSP_ERR_ABORTED);
 read_error=0;
 assert(IIC_Write(lcd,0x38,&value,1)==0);
 assert(IIC_BoardRelease(cam)==0 && closes==0 && lcd->opened);
 close_error=FSP_ERR_IN_USE;
 assert(IIC_BoardRelease(lcd)==FSP_ERR_IN_USE && lcd->opened);
 close_error=0;
 assert(IIC_BoardRelease(lcd)==0 && !lcd->opened);
 assert(IIC_BoardRelease(lcd)==FSP_ERR_NOT_OPEN);
 assert(IIC_Claim(&python_bus)==0);
 assert(IIC_BoardAcquire(&cam)==FSP_ERR_IN_USE);
 assert(IIC_Init(&python_bus)==0);
 assert(IIC_BoardAcquire(&cam)==FSP_ERR_IN_USE);
 assert(IIC_DeInit(&python_bus)==0);
 assert(IIC_BoardAcquire(&cam)==FSP_ERR_IN_USE);
 IIC_Unclaim(&python_bus);
 open_error=FSP_ERR_ABORTED;
 assert(IIC_BoardAcquire(&cam)==FSP_ERR_ABORTED);
 open_error=0;
 assert(IIC_Init(&python_bus)==0);
 assert(IIC_DeInit(&python_bus)==0);
 assert(IIC_BoardAcquire(&cam)==0);
 assert(IIC_BoardRelease(cam)==0);
 test_camera_board();
 puts("shared IIC0 and camera board tests passed");
}
