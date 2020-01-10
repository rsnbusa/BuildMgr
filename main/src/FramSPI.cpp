#include "includes.h"
#include "defines.h"
#define GLOBAL
#include "globals.h"
// file spi_caps.h in soc/esp32/include/soc has parameter SOC_SPI_MAXIMUM_BUFFER_SIZE set to 64. Max bytes in Halfduplex.
#include "soc/spi_caps.h"

#define DEBUGMQQT
#define METERSIZE	(DATAEND-BEATSTART)

FramSPI::FramSPI(void)
{
	_framInitialised = false;
	spi =NULL;
	intframWords=0;
	addressBytes=0;
	prodId=0;
	manufID=0;
}

bool FramSPI::begin(int MOSI, int MISO, int CLK, int CS,SemaphoreHandle_t *framSem)
{
	int ret;
	spi_bus_config_t 				buscfg;
	spi_device_interface_config_t 	devcfg;

	memset(&buscfg,0,sizeof(buscfg));
	memset(&devcfg,0,sizeof(devcfg));

	buscfg.mosi_io_num=MOSI;
	buscfg .miso_io_num=MISO;
	buscfg.sclk_io_num=CLK;
	buscfg.quadwp_io_num=-1;
	buscfg .quadhd_io_num=-1;
	buscfg.max_transfer_sz=10000;// useless in Half Duplex, max is set by ESPIDF in SOC_SPI_MAXIMUM_BUFFER_SIZE 64 bytes

	//Initialize the SPI bus
	ret=spi_bus_initialize(VSPI_HOST, &buscfg, 0);
	assert(ret == ESP_OK);


//	devcfg .clock_speed_hz=SPI_MASTER_FREQ_40M;              	//Clock out at 26 MHz
	devcfg .clock_speed_hz=SPI_MASTER_FREQ_26M;              	//Clock out at 26 MHz
//	devcfg .clock_speed_hz=SPI_MASTER_FREQ_8M;              	//Clock out for test in Saleae clone limited speed
	devcfg.mode=0;                                				//SPI mode 0
	devcfg.spics_io_num=CS;               						//CS pin
	devcfg.queue_size=7;                         				//We want to be able to queue 7 transactions at a time
	devcfg.flags=SPI_DEVICE_HALFDUPLEX;


	ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
	if (ret==ESP_OK)
	{
		getDeviceID(&manufID, &prodId);

		if(theConf.traceflag & (1<<FRAMD))
			printf("%sManufacturerId %04x ProductId %04x\n",FRAMDT,manufID,prodId);

		//Set write enable after chip is identified
		switch(prodId)
		{
		case 0x409:
			addressBytes=2;
			intframWords=16384;//128k
			break;
		case 0x509:
			addressBytes=2;
			intframWords=32768;//256k
			break;
		case 0x2603:
			addressBytes=2;
			intframWords=65536;//542k
			break;
		case 0x2703:
			addressBytes=3;
			intframWords=131072;//1mb
			break;
		case 0x4803:
			addressBytes=3;
			intframWords=262144;//2mb
			break;
		default:
			addressBytes=2;
			intframWords=0;
			return false;
		}

		_framInitialised = true;

		*framSem= xSemaphoreCreateBinary();
		if(*framSem)
			xSemaphoreGive(*framSem);  //SUPER important else its born locked
		else
			printf("Cant allocate Fram Sem\n");

		if(TOTALFRAM>intframWords)
		{
			printf("Not enough space for Meter Definition %d vs %d required\n",intframWords,FINTARIFA);
			return false;
		}

		devcfg.address_bits=addressBytes*8;
		devcfg.command_bits=8;

		ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);

		setWrite();// ONLY once required per datasheet
		return true;
	}
	return false;
}


int  FramSPI::sendCmd (uint8_t cmd)
{
	esp_err_t ret;
	spi_transaction_ext_t t;

	memset(&t, 0, sizeof(t));       //Zero out the transaction no need to set to 0 or null unused params

	t.base.flags= 		( SPI_TRANS_VARIABLE_CMD );
	t.base.cmd=			cmd;
	t.command_bits = 	8;

	ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit!
	return ret;

}

int  FramSPI::readStatus ( uint8_t* donde)
{
	esp_err_t ret;
	spi_transaction_ext_t t;

	memset(&t, 0, sizeof(t));       //Zero out the transaction no need to set to 0 or null unused params

	t.base.flags= 		( SPI_TRANS_VARIABLE_CMD | SPI_TRANS_USE_RXDATA );
	t.base.cmd=			MBRSPI_RDSR;
	t.base.rxlength=	8;
	t.command_bits = 	8;                                                    //zero command bits
	ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit!
    *donde=t.base.rx_data[0];
	return ret;
}

int  FramSPI::writeStatus ( uint8_t streg)
{
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	t.tx_data[0]=MBRSPI_WRSR;
	t.tx_data[1]=streg;
	t.length=16;                     //Command is 8 bits
	t.flags=SPI_TRANS_USE_TXDATA;
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
	return ret;
}

void FramSPI::setWrite()
{
	int countst=10;
	uint8_t st=0;

	while(countst>0 && st!=2)
	{
		readStatus(&st);
		st=st&2;
		if(st!=2)
		{
			sendCmd(MBRSPI_WREN);
			countst--;
		}
	}
}


int FramSPI::writeMany (uint32_t framAddr, uint8_t *valores,uint32_t son)
{
	spi_transaction_ext_t t;
	esp_err_t ret=0;
	int count,fueron;

	memset(&t,0,sizeof(t));	//Zero out the transaction no need to set to 0 or null unused params
	count=son;

	while(count>0)
	{
		fueron=				count>TXL?TXL:count;
		t.base.flags= 		( SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD );
		t.base.addr = 		framAddr;
		t.base.length = 	fueron*8;
		t.base.tx_buffer = 	valores;
		t.base.cmd=			MBRSPI_WRITE;
		t.command_bits = 	8;
		t.address_bits = 	addressBytes*8;
		ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit and wait!

		count				-=fueron; 	// reduce bytes processed
		framAddr			+=fueron;  	// advance Address by fueron bytes processed
		valores				+=fueron;  	// advance Buffer to write by processed bytes
	}
	return ret;
}

int FramSPI::readMany (uint32_t framAddr, uint8_t *valores, uint32_t son)
{
	esp_err_t ret=0;
	spi_transaction_ext_t t;
	int cuantos,fueron;

	memset(&t, 0, sizeof(t));	//Zero out the transaction no need to set to 0 or null unused params

	cuantos=son;
	while(cuantos>0)
	{
		fueron=				cuantos>TXL?TXL:cuantos;

		t.base.flags= 		( SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD );
		t.base.addr = 		framAddr;
		t.base.cmd=			MBRSPI_READ;
		t.command_bits = 	8;
		t.address_bits = 	addressBytes*8;
		t.base.rx_buffer=	valores;
		t.base.rxlength=	fueron*8;
		ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);	//Transmit and wait

		cuantos				-=fueron;
		framAddr			+=fueron;
		valores				+=fueron;
	}
	return ret;
}

int FramSPI::write8 (uint32_t framAddr, uint8_t value)
{
	esp_err_t ret;
	spi_transaction_ext_t t;

	memset(&t,0,sizeof(t));	//Zero out the transaction no need to set to 0 or null unused params

	t.base.tx_data[0]=	value;
	t.base.flags= 		( SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD| SPI_TRANS_USE_TXDATA );
	t.base.addr = 		framAddr;
	t.base.length = 	8;
	t.base.cmd=			MBRSPI_WRITE;
	t.command_bits = 	8;
	t.address_bits = 	addressBytes*8;
	ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit and wait!
	return ret;
}

int FramSPI::read8 (uint32_t framAddr,uint8_t *donde)
{
	spi_transaction_ext_t t;
	int ret;

	memset(&t, 0, sizeof(t));       //Zero out the transaction no need to set to 0 or null unused params

	t.base.flags= 		( SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD| SPI_TRANS_USE_RXDATA );
	t.base.addr = 		framAddr;                                         //set address
	t.base.cmd=			MBRSPI_READ;
	t.command_bits = 	8;
	t.address_bits = 	addressBytes*8;
	t.base.rxlength=	8;
	ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit!
	*donde=t.base.rx_data[0];
	return ret;
}

void FramSPI::getDeviceID(uint16_t *manufacturerID, uint16_t *productID)
{
	spi_transaction_ext_t t;

	memset(&t, 0, sizeof(t));       //Zero out the transaction no need to set to 0 or null unused params

	t.base.flags= 		( SPI_TRANS_VARIABLE_CMD| SPI_TRANS_USE_RXDATA );
	t.base.cmd=			MBRSPI_RDID;
	t.command_bits = 	8;
	t.base.rxlength=	32;
	spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit and wait!

	// Shift values to separate manuf and prod IDs
	// See p.10 of http://www.fujitsu.com/downloads/MICRO/fsa/pdf/products/memory/fram/MB85RC256V-DS501-00017-3v0-E.pdf
	*manufacturerID=(t.base.rx_data[0]<<8)+t.base.rx_data[1];
	*productID=(t.base.rx_data[2]<<8)+t.base.rx_data[3];
}



int FramSPI::format(uint8_t valor, uint8_t *lbuffer,uint32_t len,bool all)
{
	uint32_t add=0;
	int count=intframWords,ret;

	uint8_t *buffer=(uint8_t*)malloc(len);
	if(!buffer)
	{
		printf("Failed format buf\n");
		return -1;
	}

	while (count>0)
	{
		if(lbuffer!=NULL)
				memcpy(buffer,lbuffer,len); //Copy whatever was passed
			else
				memset(buffer,valor,len);  //Should be done only once

		if (count>len)
		{
		//	printf("Format add %d len %d count %d val %d\n",add,len,count,valor);
			ret=writeMany(add,buffer,len);
			if (ret!=0)
				return ret;
		}
		else
		{
		//	printf("FinalFormat add %d len %d val %d\n",add,count,valor);
			ret=writeMany(add,buffer,count);
			if (ret!=0)
				return ret;
		}
		count-=len;
		add+=len;
//		delay(5);
	}
	free(buffer);
	return ESP_OK;
}

int FramSPI::formatSlow(uint8_t valor)
{
	uint32_t add=0;
	int count=intframWords;
	while (count>0)
	{
		write8(add,valor);
		count--;
		add++;
	}
	return ESP_OK;
}


int FramSPI::formatMeter(uint8_t cual)
{
	uint32_t add;
	if (cual>MAXDEVSS)
		return -1;		//OB

	int count=DATAEND-BEATSTART,ret;
	add=count*cual+BEATSTART;

	void *buf=malloc(count);
	if(!buf)
		return -1;
	memset(buf,0,count);
	ret=writeMany(add,(uint8_t*)buf,count);
	return ret;

}

int FramSPI::read_tarif_bytes(uint32_t add,uint8_t*  donde,uint32_t cuantos)
{
	int ret;
	ret=readMany(add,donde,cuantos);
	return ret;
}

int FramSPI::read_tarif_day(uint16_t dia,uint8_t*  donde) //Read 24 Hours of current Day(0-365)
{
	int ret;
	uint32_t add=TARIFADIA+dia*24*MWORD;
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sR TarDay %d Add %d\n",FRMCMDT,dia,add);
	ret=read_tarif_bytes(add,donde,24*MWORD);
	return ret;
}

int FramSPI::read_tarif_hour(uint16_t dia,uint8_t hora,uint8_t*  donde) //Read specific Hour in a Day. Day 0-365 and Hour 0-23
{
	int ret;
	uint32_t add=TARIFADIA+dia*24*MWORD+hora;
//	if(theConf.traceflag & (1<<FRMCMD))
//	printf("%sR TarHour Day %d Hour %d Add %d\n",FRMCMDT,dia,hora,add);
	ret=read_tarif_bytes(add,donde,MWORD);
	return ret;
}

// Meter Data Management

int FramSPI::write_bytes(uint8_t meter,uint32_t add,uint8_t*  desde,uint32_t cuantos)
{
	int ret;
	if (meter>MAXDEVSS)
		return -1;		//OB
	add+=METERSIZE*meter;
	ret=writeMany(add,desde,cuantos);
	return ret;
}

int FramSPI::read_bytes(uint8_t meter,uint32_t add,uint8_t*  donde,uint32_t cuantos)
{
	if (meter>MAXDEVSS)
		return -1;		//OB
	add+=METERSIZE*meter;
	int ret;
	ret=readMany(add,donde,cuantos);
	return ret;
}

int FramSPI::write_recover(scratchTypespi value)
{
	uint32_t add=SCRATCH;
	uint8_t*  desde=(uint8_t* )&value;
	uint8_t cuantos=sizeof(scratchTypespi);
	int ret=writeMany(add,desde,cuantos);
	return ret;
}

int FramSPI::write_cycle(uint8_t mes, uint32_t  value)
{
	int ret;
	uint32_t badd=MCYCLE+mes*LLONG;
	if(mes>11)
		return -1;// month OB
	ret=writeMany(badd,(uint8_t*)&value,LLONG);
	return ret;
}

int FramSPI::write_beat(uint8_t meter, uint32_t value)
{
	int ret;
	if (meter>MAXDEVSS)
		return -1;		//OB
	uint32_t badd=BEATSTART;
	ret=write_bytes(meter,badd,(uint8_t* )&value,LLONG);
	return ret;
}

int FramSPI::write_lifedate(uint8_t meter, uint32_t value)
{
	int ret;
	uint32_t badd=LIFEDATE;
	if (meter>MAXDEVSS)
		return -1;		//OB

	ret=write_bytes(meter,badd,(uint8_t* )&value,LLONG);
	return ret;
}

int FramSPI::write_lifekwh(uint8_t meter, uint32_t value)
{
	int ret;
	uint32_t badd=LIFEKWH;
	if (meter>MAXDEVSS)
		return -1;		//OB
	ret=write_bytes(meter,badd,(uint8_t* )&value,LLONG);
	return ret;
}

int FramSPI::write_month(uint8_t meter,uint8_t month,uint16_t value)
{
	int ret;

	uint32_t badd=MONTHSTART+month*MWORD;
	if (meter>MAXDEVSS || month>11)
		return -1;		//OB

	ret=write_bytes(meter,badd,(uint8_t* )&value,MWORD);
	return ret;
}

int FramSPI::write_monthraw(uint8_t meter,uint8_t month,uint16_t value)
{
	int ret;
	uint32_t badd=MONTHRAW+month*MWORD;
	if (meter>MAXDEVSS || month>11)
		return -1;		//OB

	ret=write_bytes(meter,badd,(uint8_t* )&value,MWORD);
	return ret;
}

int FramSPI::write_day(uint8_t meter,uint16_t days,uint16_t value)
{
	int ret;
	uint32_t badd=DAYSTART+days*MWORD;
	if (meter>MAXDEVSS || days>366)
		return -1;		//OB

	ret=write_bytes(meter,badd,(uint8_t* )&value,MWORD);
	return ret;
}

int FramSPI::write_dayraw(uint8_t meter,uint16_t days,uint16_t value)
{
	int ret;

	uint32_t badd=DAYRAW+days*MWORD;
	if (meter>MAXDEVSS || days>366)
		return -1;		//OB

	ret=write_bytes(meter,badd,(uint8_t* )&value,MWORD);
	return ret;
}

int FramSPI::write_hour(uint8_t meter,uint16_t days,uint8_t hora,uint8_t value)
{
	int ret;

	uint32_t badd=HOURSTART+(days*24)+hora;
	if (meter>MAXDEVSS || days>366 || hora>23)
		return -1;		//OB

	ret=write_bytes(meter,badd,(uint8_t* )&value,1);
	return ret;
}

int FramSPI::write_hourraw(uint8_t meter,uint16_t days,uint8_t hora,uint8_t value)
{
	int ret;
	uint32_t badd=HOURRAW+(days*24)+hora;
	if (meter>MAXDEVSS || days>366 || hora>23)
		return -1;		//OB

	ret=write_bytes(meter,badd,(uint8_t* )&value,1);
	return ret;
}

int FramSPI::read_cycle(uint8_t mes, uint8_t*  value)
{
	int ret;
	uint32_t badd=MCYCLE+mes*LLONG;
	if (mes>11)
		return -1;		//OB
	ret=readMany(badd,value,LLONG);

	return ret;
}

int FramSPI::read_lifedate(uint8_t meter, uint8_t*  value)
{
	int ret;
	uint32_t badd=LIFEDATE;
	if (meter>MAXDEVSS)
		return -1;		//OB
	ret=read_bytes(meter,badd,value,LLONG);
	return ret;
}

int FramSPI::read_recover(scratchTypespi* aqui)
{
	int ret;
	uint8_t*  donde=(uint8_t* )aqui;
	uint16_t cuantos = sizeof(scratchTypespi);
	uint32_t add=SCRATCH;
	ret=readMany(add,donde,cuantos);
	if (ret!=0)
		printf("Read REcover error %d\n",ret);
	return ret;
}


int FramSPI::read_lifekwh(uint8_t meter, uint8_t*  value)
{
	int ret;
	uint32_t badd=LIFEKWH;
	if (meter>MAXDEVSS )
		return -1;		//OB
	ret=read_bytes(meter,badd,value,LLONG);
	return ret;
}

int FramSPI::read_beat(uint8_t meter, uint8_t*  value)
{
	int ret;
	uint32_t badd=BEATSTART;
	if (meter>MAXDEVSS )
			return -1;		//OB
	ret=read_bytes(meter,badd,value,LLONG);

	return ret;
}

int FramSPI::read_month(uint8_t meter,uint8_t month,uint8_t*  value)
{
	int ret;
	uint32_t badd=MONTHSTART+month*MWORD;
	if (meter>MAXDEVSS || month>11)
			return -1;		//OB
	ret=read_bytes(meter,badd,value,MWORD);

	return ret;
}

int FramSPI::read_monthraw(uint8_t meter,uint8_t month,uint8_t*  value)
{
	int ret;
	uint32_t badd=MONTHRAW+month*MWORD;
	if (meter>MAXDEVSS || month>11)
			return -1;		//OB
	ret=read_bytes(meter,badd,value,MWORD);

	return ret;
}

int FramSPI::read_day(uint8_t meter,uint16_t days,uint8_t*  value)
{
	int ret;
	uint32_t badd=DAYSTART+days*MWORD;
	if (meter>MAXDEVSS || days>366)
			return -1;		//OB
	ret=read_bytes(meter,badd,value,MWORD);

	return ret;
}

int FramSPI::read_dayraw(uint8_t meter,uint16_t days,uint8_t*  value)
{
	int ret;
	uint32_t badd=DAYRAW+days*MWORD;
	if (meter>MAXDEVSS || days>366)
			return -1;		//OB
	ret=read_bytes(meter,badd,value,MWORD);

	return ret;
}

int FramSPI::read_hour(uint8_t meter,uint16_t days,uint8_t hora,uint8_t*  value)
{
	int ret;
	uint32_t badd=HOURSTART+(days*24)+hora;
	if (meter>MAXDEVSS || days>366 || hora>23)
			return -1;		//OB
	ret=read_bytes(meter,badd,value,1);
	return ret;
}
int FramSPI::read_hourraw(uint8_t meter,uint16_t days,uint8_t hora,uint8_t*  value)
{
	int ret;
	uint32_t badd=HOURRAW+(days*24)+hora;
	if (meter>MAXDEVSS || days>366 || hora>23)
			return -1;		//OB
	ret=read_bytes(meter,badd,value,1);
	return ret;
}
