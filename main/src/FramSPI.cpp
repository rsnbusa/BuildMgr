#include "includes.h"
#include "defines.h"
#define GLOBAL
// file spi_caps.h in soc/esp32/include/soc has parameter SOC_SPI_MAXIMUM_BUFFER_SIZE set to 64. Max bytes in Halfduplex. changed to 1024
#include "soc/spi_caps.h"
extern void delay(uint32_t a);

#define DEBUGMQQT
/*========================================================================*/
/*                            CONSTRUCTORS                                */
/*========================================================================*/

/**************************************************************************/
/*!
 Constructor
 */
/**************************************************************************/
FramSPI::FramSPI(void)
{
	_framInitialised = false;
	spi =NULL;
	intframWords=0;
	addressBytes=0;
	prodId=0;
	manufID=0;
}

/*========================================================================*/
/*                           PUBLIC FUNCTIONS                             */
/*========================================================================*/

/**************************************************************************/
/*!
 Send SPI Cmd. JUST the cmd.
 */
/**************************************************************************/
int  FramSPI::sendCmd (uint8_t cmd)
{
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	t.tx_data[0]=cmd;
	t.length=8;                     //Command is 8 bits
	t.flags=SPI_TRANS_USE_TXDATA;
	// Nov 22/2019 polling mode new cmd. Will block until transmitted
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!

	return ret;

}


/**************************************************************************/
/*!
 Status SPI Cmd.
 */
/**************************************************************************/
int  FramSPI::readStatus ( uint8_t* donde)
{
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	t.length=8;                     //Command is 8 bits
	t.rxlength=8;
	t.tx_data[0]=MBRSPI_RDSR;
	t.flags=SPI_TRANS_USE_TXDATA|SPI_TRANS_USE_RXDATA;
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    *donde=t.rx_data[0];
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
			delay(1);
		}
	}

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

	devcfg .clock_speed_hz=SPI_MASTER_FREQ_26M;              	//Clock out at 26 MHz
//	devcfg .clock_speed_hz=8000000;              	//Clock out for test in Saleae limited speed
	devcfg.mode=0;                                	//SPI mode 0
	devcfg.spics_io_num=CS;               			//CS pin
	devcfg.queue_size=7;                         	//We want to be able to queue 7 transactions at a time
	devcfg.flags=SPI_DEVICE_HALFDUPLEX;

	ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
	if (ret==ESP_OK)
	{
		getDeviceID(&manufID, &prodId);

		//Set write enable after chip is identified
		switch(prodId)
		{
		case 0x409:
			addressBytes=2;
			intframWords=16384;
			break;
		case 0x509:
			addressBytes=2;
			intframWords=32768;
			break;
		case 0x2603:
			addressBytes=2;
			intframWords=65536;
			break;
		case 0x2703:
			addressBytes=3;
			intframWords=131072;
			break;
		case 0x4803:
			addressBytes=3;
			intframWords=262144;
			break;
		default:
			addressBytes=2;
			intframWords=0;
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

		setWrite();// I guess this should be done only once, but i dont know
		return true;
	}
	return false;
}

int FramSPI::writeMany (uint32_t framAddr, uint8_t *valores,uint32_t son)
{
	spi_transaction_t t;
	esp_err_t ret=0;
	int count,fueron; //retries
	uint8_t lbuf[TXL+4];

	memset(&t,0,sizeof(t));
	count=son;

	//make sure its writable. Just once
//	setWrite();

	while(count>0)
	{
		lbuf[0]=MBRSPI_WRITE;
		if(addressBytes==2)
		{
			lbuf[1]=(framAddr & 0xff00)>>8;
			lbuf[2]=framAddr& 0xff;
			fueron=count>TXL+1?TXL+1:count;  //29= 32 - 3 command bytes MAX tx bytes in Half Duplex 32 bytes
			t.length=(fueron+3)*8;                     //Command is  (bytes+3) *8 =32 bits
			memcpy(&lbuf[3],valores,fueron); //should check that son is less or equal to bbuffer size
		}
		else
		{
			lbuf[1]=(framAddr & 0xff0000)>>16;
			lbuf[2]=(framAddr & 0xff00)>>8;
			lbuf[3]=framAddr & 0xff;
			fueron=count>TXL?TXL:count;	//= 32 - 4 command bytes MAX tx bytes in Half Duplex 32 bytes
			t.length=(fueron+4)*8;                     //Command is  (bytes+3) *8 =32 bits
			memcpy(&lbuf[4],valores,fueron);
		}

		t.tx_buffer=lbuf;
		ret=spi_device_polling_transmit(spi, &t);  //Transmit!
		count-=fueron; // reduce bytes processed
		framAddr+=fueron;  //advance Address by fueron bytes processed
		valores+=fueron;  // advance Buffer to write by processed bytes
	}
	return ret;
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


int FramSPI::formatMeter(uint8_t cual, uint8_t *buffer,uint16_t len)
{
	uint32_t add;
	int count=DATAEND-BEATSTART,ret;
	add=count*cual+BEATSTART;
	while (count>0)
	{
		if (count>len)
		{
			memset(buffer,0,len);
			ret=writeMany(add,buffer,len);
			if (ret!=0)
				return ret;
		}
		else
		{
			memset(buffer,0,count);
			ret=writeMany(add,buffer,count);
			if (ret!=0)
				return ret;
		}
		count-=len;
		add+=len;
		delay(2);
	}
	return ESP_OK;

}

int FramSPI::readMany (uint32_t framAddr, uint8_t *valores,uint32_t son)
{
	esp_err_t ret=0;
	spi_transaction_t t;
	uint8_t tx[4];
	int cuantos,fueron;
	memset(&t, 0, sizeof(t));

	tx[0]=MBRSPI_READ;

	cuantos=son;
	while(cuantos>0)
	{
		memset(&t, 0, sizeof(t));
		if(addressBytes==2)
		{
			tx[1]=(framAddr & 0xff00)>>8;
			tx[2]=framAddr & 0xff;
			t.length=24;
		}
		else
		{
			tx[1]=(framAddr & 0xff0000) >>16;
			tx[2]=(framAddr & 0xff00)>>8;
			tx[3]=framAddr & 0xff;
			t.length=32;
		}
		t.tx_buffer=&tx;
		fueron=cuantos>TXL?TXL:cuantos;
		t.rxlength=fueron*8;
		t.rx_buffer=valores;
		ret=spi_device_polling_transmit(spi, &t);
		cuantos-=fueron;
		framAddr+=fueron;
		valores+=fueron;
	}
	return ret;
}

int FramSPI::write8 (uint32_t framAddr, uint8_t value)
{

	esp_err_t ret;
	spi_transaction_t t;
	uint8_t data[5];
	int count=20; //retries

	memset(&t,0,sizeof(t));
//	setWrite();

	if (count==0)
		return -1; // error internal cant get a valid status. Defective chip or whatever

	data[0]=MBRSPI_WRITE;
	if(addressBytes==2)
	{
		data[1]=(framAddr &0xff00)>>8;
		data[2]=framAddr& 0xff;
		data[3]=value;
		t.length=32;                     //Command is 4 bytes *8 =32 bits
	}
	else
	{
		data[1]=(framAddr & 0xff0000)>>16;
		data[2]=(framAddr& 0xff00)>>8;
		data[3]=framAddr& 0xff;
		data[4]=value;
		t.length=40;                     //Command is 5 bytes *8 =402 bits
	}

	t.tx_buffer=&data;
	t.rxlength=0;
	t.rx_buffer=NULL;
	ret=spi_device_polling_transmit(spi, &t);  //Transmit!
	return ret;
}

int FramSPI::read8 (uint32_t framAddr,uint8_t *donde)
{
	spi_transaction_t t;
	uint8_t data[4];
	int ret;
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	data[0]=MBRSPI_READ;
	if(addressBytes==2)
	{
		data[1]=(framAddr &0xff00)>>8;
		data[2]=framAddr& 0xff;
		t.length=24;                     //Command is 3 btyes *8 =24 bits
	}
	else
	{
		data[1]=(framAddr & 0xff0000)>>16;
		data[2]=(framAddr& 0xff00)>>8;
		data[3]=framAddr& 0xff;
		t.length=32;                     //Command is 4 btyes *8 =32 bits
	}

	t.rxlength=8;
	t.rx_buffer=donde;
	t.tx_buffer=&data;
	ret=spi_device_polling_transmit(spi, &t);  //Transmit!
	return ret;


}

void FramSPI::getDeviceID(uint16_t *manufacturerID, uint16_t *productID)
{
	uint8_t aqui[4] = { 0 };
	spi_transaction_t t;
	uint8_t data;

	memset(&t, 0, sizeof(t));       //Zero out the transaction
	data=MBRSPI_RDID;
	t.length=8;                     //Command is 4 byes *8 =32 bits
	t.tx_buffer=&data;
	t.rxlength=32;
	t.rx_buffer=&aqui;
	spi_device_polling_transmit(spi, &t);  //Transmit!

	// Shift values to separate manuf and prod IDs
	// See p.10 of http://www.fujitsu.com/downloads/MICRO/fsa/pdf/products/memory/fram/MB85RC256V-DS501-00017-3v0-E.pdf
	*manufacturerID=(aqui[0]<<8)+aqui[1];
	*productID=(aqui[2]<<8)+aqui[3];

}

uint16_t date2daysSPI(uint16_t y, uint8_t m, uint8_t d) {
	uint8_t daysInMonth [12] ={ 31,28,31,30,31,30,31,31,30,31,30,31 };//offsets 0,31,59,90,120,151,181,212,243,273,304,334, +1 if leap year
	uint16_t days = d;
	for (uint8_t i = 0; i < m; i++)
		days += daysInMonth[ i];
	if (m > 1 && y % 4 == 0)
		++days;
	return days ;

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
	ret=read_tarif_bytes(add,donde,24*MWORD);
	return ret;
}

int FramSPI::read_tarif_hour(uint16_t dia,uint8_t hora,uint8_t*  donde) //Read specific Hour in a Day. Day 0-365 and Hour 0-23
{
	int ret;
	uint32_t add=TARIFADIA+dia*24*MWORD+hora;
	ret=read_tarif_bytes(add,donde,MWORD);
	return ret;
}

// Meter Data Management

int FramSPI::write_bytes(uint8_t meter,uint32_t add,uint8_t*  desde,uint32_t cuantos)
{
	int ret;
	//add+=DATAEND*meter+SCRATCH;
	add+=DATAEND*meter;
	ret=writeMany(add,desde,cuantos);
	return ret;
}

int FramSPI::read_bytes(uint8_t meter,uint32_t add,uint8_t*  donde,uint32_t cuantos)
{
	//add+=DATAEND*meter+SCRATCH;
	add+=DATAEND*meter;
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

int FramSPI::write_beat(uint8_t medidor, uint32_t value)
{
	int ret;
	uint32_t badd=BEATSTART;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LLONG);
	return ret;
}

int FramSPI::write_lifedate(uint8_t medidor, uint32_t value)
{
	int ret;
	uint32_t badd=LIFEDATE;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LLONG);
	return ret;
}

int FramSPI::write_lifekwh(uint8_t medidor, uint32_t value)
{
	int ret;
	uint32_t badd=LIFEKWH;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LLONG);
	return ret;
}

int FramSPI::write_month(uint8_t medidor,uint8_t month,uint16_t value)
{
	int ret;
	uint32_t badd=MONTHSTART+month*MWORD;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,MWORD);
	return ret;
}

int FramSPI::write_monthraw(uint8_t medidor,uint8_t month,uint16_t value)
{
	int ret;
	uint32_t badd=MONTHRAW+month*MWORD;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,MWORD);
	return ret;
}

int FramSPI::write_day(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint16_t value)
{
	int ret;
	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=DAYSTART+days*MWORD;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,MWORD);
	return ret;
}

int FramSPI::write_dayraw(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint16_t value)
{
	int ret;
	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=DAYRAW+days*MWORD;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,MWORD);
	return ret;
}

int FramSPI::write_hour(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t hora,uint8_t value)
{
	int ret;
	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=HOURSTART+(days*24)+hora;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,1);
	return ret;
}

int FramSPI::write_hourraw(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t hora,uint8_t value)
{
	int ret;
	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=HOURRAW+(days*24)+hora;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,1);
	return ret;
}

int FramSPI::read_lifedate(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=LIFEDATE;
	ret=read_bytes(medidor,badd,value,LLONG);
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


int FramSPI::read_lifekwh(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=LIFEKWH;
	ret=read_bytes(medidor,badd,value,LLONG);
	return ret;
}

int FramSPI::read_beat(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=BEATSTART;
	ret=read_bytes(medidor,badd,value,LLONG);
	return ret;
}

int FramSPI::read_month(uint8_t medidor,uint8_t month,uint8_t*  value)
{
	int ret;
	uint32_t badd=MONTHSTART+month*MWORD;
	ret=read_bytes(medidor,badd,value,MWORD);
	return ret;
}

int FramSPI::read_monthraw(uint8_t medidor,uint8_t month,uint8_t*  value)
{
	int ret;
	uint32_t badd=MONTHRAW+month*MWORD;
	ret=read_bytes(medidor,badd,value,MWORD);
	return ret;
}

int FramSPI::read_day(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t*  value)
{
	int ret;
	int days=date2daysSPI(yearl,month,dia);
	uint32_t badd=DAYSTART+days*MWORD;
	ret=read_bytes(medidor,badd,value,MWORD);
	return ret;
}

int FramSPI::read_dayraw(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t*  value)
{
	int ret;
	int days=date2daysSPI(yearl,month,dia);
	uint32_t badd=DAYRAW+days*MWORD;
	ret=read_bytes(medidor,badd,value,MWORD);
	return ret;
}

int FramSPI::read_hour(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t hora,uint8_t*  value)
{
	int ret;
	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=HOURSTART+(days*24)+hora;
	ret=read_bytes(medidor,badd,value,1);
	return ret;
}
int FramSPI::read_hourraw(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t hora,uint8_t*  value)
{
	int ret;
	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=HOURRAW+(days*24)+hora;
	ret=read_bytes(medidor,badd,value,1);
	return ret;
}
