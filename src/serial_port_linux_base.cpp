#include "serial_port/serial_port_linux_base.h"
#include "serial_port/serial_port_params.h"
#include "config.h"
#include "utils.h"

#ifdef os_is_linux

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <iostream>
using namespace std;



#ifdef has_cxx_11

#ifdef use_fmt
	#include "fmt/format.h"
#endif //! use_fmt

namespace lib_sp
{
	/**
	*	@brief: constructor
	*/
	lib_sp::serial_port_linux_base::serial_port_linux_base() noexcept
	{
		_sp_param.zero();
	}


	/**
	*	@brief: descontructor
	*/
	serial_port_linux_base::~serial_port_linux_base() noexcept
	{
		// to close serial port
		if (is_opened())
		{
			try
			{
				this->close();
			}
			catch (...)
			{
			}
		}
	}

	/**
	*	@brief: to initialize parameters
	*/
	int serial_port_linux_base::init(const sp_prop& spp, irecv_data* precv_data /*= nullptr*/) noexcept
	{
		int ret_val = 0;

		// 1. check parameters 
		ret_val = sp_params_check::get_instance().is_right(std::move(spp));
		if (0 != ret_val)
			return ret_val;

		// 2. 
		_sp_param._spp = spp;
		if (nullptr != precv_data || NULL != precv_data)
			_sp_param._thread._precv_data = precv_data;

		return ret_val;
	}

	/**
	*	@brief:
	*/
	int serial_port_linux_base::open() noexcept
	{
		int ret_val = 0;

		// 1. serial port has been opened
		if (is_opened())
		{
			ret_val = -1;
			return ret_val;
		}

		// 2.try to open serial port
		sp_param_linux &base_param = _sp_param;
		sp_prop &spp = _sp_param._spp;

		/*
			O_RDONLY ��ֻ����ʽ��   
			O_WRONLY ��ֻд��ʽ��   	
			O_RDWR �Կɶ���д��ʽ��

			O_CREAT ����ļ��������򴴽����ļ�
			O_EXCL ���ʹ��O_CREATѡ�����ļ����ڣ��򷵻ش�����Ϣ
			O_NOCTTY ����ļ�Ϊ�նˣ���ô�ն˲����Ե���openϵͳ���õ��Ǹ����̵Ŀ����ն�
			O_TRUNC ����ļ��Ѿ�������ɾ���ļ���ԭ������
			O_APPEND ��׷�ӵķ�ʽ��
		*/
		base_param._fd = ::open(spp._name.c_str(), O_RDWR  | O_NOCTTY | O_NDELAY);

		// 3. failed to open that
		if (invalid_value == base_param._fd)
		{	
			ret_val = errno;
			return ret_val;

		}

		// 4. to set port block or unblock
		if (0 > fcntl(base_param._fd, F_SETFL, 0))
		{
			// ret_val = -3;
			ret_val = errno;
			return ret_val;
		}

		// 5. try to set params of this port, but faile
		if (0 != set_port_params())
		{
			// ret_val = -4;
			ret_val = errno;
			return ret_val;
		}

		return ret_val;
	}

	/**
	*	@brief:
	*/
	int serial_port_linux_base::close() noexcept
	{
		int ret_val = 0;

		if (!is_opened())
		{
			ret_val = -1;
		}
		else
		{
			try
			{

			}
			catch(...)
			{
				// call system func to close port
				::close(_sp_param._fd);
				_sp_param._fd	= invalid_value;

				ret_val = errno;
			}
		}

		return ret_val;
	}

	/**
	*	@brief:
	*/
	void serial_port_linux_base::set_read_notify(const unsigned int len) noexcept
	{
		_sp_param._others._min_byte_read_notify = len;
	}

	/**
	*	@brief: to check serial port status
	*/
	bool serial_port_linux_base::is_opened() noexcept
	{
		return (invalid_value == _sp_param._fd) ? false : true;
	}

	/**
	*	@brief:
	*/
	int serial_port_linux_base::send(const char* pdata, const unsigned int data_len)
	{
		// 0.
		if (nullptr == pdata || NULL == pdata || 0 == data_len)
		{
			return -1;
		}

		// 1. comm doesnt opened
		if (!is_opened())
		{
			return -2;
		}


		int ret_val = 0;
		try
		{
			// 2. to send data , ret_val = -1 ? X > 0
			ret_val = write(_sp_param._fd, pdata, data_len);	
		}
		catch(...)
		{
			ret_val = errno;
		}

		return ret_val;
	}

	/**
	*	@brief:
	*/
	std::string serial_port_linux_base::get_version() noexcept
	{
		return utils::helper::str_format("{0}.{1}.{2}.{3}", version_1, version_2, version_3, version_4);
	}

	/**
	*	@brief:
	*/
	lib_sp::list_sp_name serial_port_linux_base::get_available_serial_port() noexcept
	{
		lib_sp::list_sp_name list_name;
		return list_name;
	}

	/**
	*	@brief: to set base params of this port
	*/
	int serial_port_linux_base::set_port_params()
	{


		/**
		* {unsigned short c_iflag;  ����ģʽ��־
		* unsigned short c_oflag;  	���ģʽ��־
		* unsigned short c_cflag;  	����ģʽ��־
		* unsigned short c_lflag; 	����ģʽ��־�򱾵�ģʽ��־��ֲ�ģʽ
		* unsigned char c_line; 	�п���line discipline 
		* unsigned char c_cc[NCC];  �����ַ�����,���п��Ը��ĵ������ַ�
		*/
		int ret_val;
		struct termios options;
		sp_param_linux& base_param = _sp_param;
		sp_prop& spp = _sp_param._spp;

		// 1. to read props of this port
		//----------------------------------------------------------------------------------------
		ret_val = tcgetattr(base_param._fd, &options);

		// failed to read prop
		if (ret_val < 0)
		{
			ret_val = -1;
			return ret_val;
		}

		// 2. to set options
		//----------------------------------------------------------------------------------------
		speed_t baud = base_param._spp._baud_rate;// sp_params_check::get_instance().get_baud(base_param._spp._baud_rate);
		cfsetispeed(&options, baud);
		cfsetospeed(&options, baud);

		// 3.data bits
		//----------------------------------------------------------------------------------------
		switch (base_param._spp._data_bits)
		{
		case lib_sp::data_bits_5:
			options.c_cflag &= ~CSIZE;
			options.c_cflag |= CS5;
			break;

		case lib_sp::data_bits_6:
			options.c_cflag &= ~CSIZE;
			options.c_cflag |= CS6;
			break;

		case lib_sp::data_bits_7:
			options.c_cflag &= ~CSIZE;
			options.c_cflag |= CS7;
			break;

		case lib_sp::data_bits_8:
			options.c_cflag &= ~CSIZE;
			options.c_cflag |= CS8;
			break;

		default:
			break;
		}

		// 4. stop bits
		//----------------------------------------------------------------------------------------
		switch (base_param._spp._stop_bits)
		{
		case lib_sp::stop_1:
			options.c_cflag &= ~CSTOPB;
			break;

		case lib_sp::stop_1_5:
			options.c_cflag &= ~CSTOPB;
			break;

		case lib_sp::stop_2:
			options.c_cflag &= ~CSTOPB;
			break;

		default:
			break;
		}

		// 5. flow control
		//----------------------------------------------------------------------------------------
		switch (base_param._spp._flow_ctl)
		{
		case lib_sp::flow_ctl_none:
			options.c_cflag &= ~CRTSCTS;
			break;

		case lib_sp::flow_ctl_hardware:
			options.c_cflag |= CRTSCTS;
			break;

		case lib_sp::flow_ctl_software:
			options.c_cflag |= IXON | IXOFF | IXANY;
			break;

		default:
			break;
		}

		// 6. parity
		//----------------------------------------------------------------------------------------
		switch (base_param._spp._parity)
		{
		case lib_sp::parity_none:
			options.c_cflag &= ~PARENB;		
			options.c_cflag &= ~INPCK;		
			break;

		case lib_sp::parity_odd:
			options.c_cflag |= PARENB;		
			options.c_cflag |= PARODD;		
			options.c_cflag |= INPCK;		
			options.c_cflag |= ISTRIP;		
			break;

		case lib_sp::parity_even:
			options.c_cflag |= PARENB;		
			options.c_cflag &= ~PARODD;		
			options.c_cflag |= INPCK;		
			options.c_cflag |= ISTRIP;		
			break;

		case lib_sp::parity_space:
			options.c_cflag &= ~PARENB;		
			options.c_cflag &= ~CSTOPB;		
			break;


		default:
			break;
		}

		
		options.c_oflag &= ~OPOST;				
			
		// ??????????????
		options.c_lflag &= ~(ICANON		|		
								ECHO	|		
								ECHOE	|		
								ISIG);			

		options.c_iflag &= ~(BRKINT		|		
								ICRNL	|		
								INPCK	|		
								ISTRIP	|		
								IXON);			


		options.c_cc[VTIME]		= 0;			
		options.c_cc[VMIN]		= 1;			

		tcflush(base_param._fd, TCIFLUSH);

		// to set config of serial port
		ret_val = tcsetattr(base_param._fd, TCSANOW, &options);

		// 4. set configure failed
		if (0 > ret_val)
		{
			// 
			ret_val = -2;
		}

		return ret_val;
	}

}


#endif // has_cxx_11
#endif //!os_is_linux
