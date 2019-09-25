/*! @file                     Axis.h
 *  @brief                    Header file of AXIS DLL application 
 *  @date                     Create on: 2016.08.17
 *  @author                   Lingyuan, Hsu
 *  @version         1.01
 *  @note
 */

//---------------------------------------------------------------------------
// Define
//---------------------------------------------------------------------------
#define ERR_NO_ERROR				 0
#define ERR_NO_OBJECT				 1
#define ERR_WRONG_MOTOR_TYPE		 2
#define ERR_WRONG_UNIT_TYPE			 3
#define ERR_WRONG_DRIVE_TYPE		 4
#define ERR_DISCONNECT				10
#define ERR_NO_SLAVE				11
#define ERR_DUPLICATE_AXIS_NAME		12
#define ERR_MISMATCH_AXIS_NAME		13
#define ERR_GET_VAR_ERROR			20
#define ERR_SET_VAR_ERROR			21
#define ERR_GET_STATE_ERROR			22
#define ERR_SET_STATE_ERROR			23
#define ERR_RUN_PDL_ERROR			24
#define ERR_SET_WRONG_INPUT_FUNC	25
#define ERR_KILL_TASK_ERROR         26
#define ERR_TASK_NUM_OVER_LIMIT     27
#define ERR_TASK_NOT_RUN			28
#define ERR_WAIT_TIMEOUT			30
#define ERR_NOT_HOMED_YET			31

//---------------------------------------------------------------------------
// Enum
//---------------------------------------------------------------------------
typedef enum EnumUnitType
{
	eUT_ROTARY = 0,
	eUT_LINEAR = 1
} ENUM_UNIT_TYPE;

typedef enum EnumMotorType
{
	eMT_LM = 0,
	eMT_DD = 1,
	eMT_AC = 2
} ENUM_MOTOR_TYPE;

typedef enum EnumDriveType
{
	eDT_D1 = 0,
	eDT_D2 = 1,
	eDT_D1N = 2
} ENUM_DRIVE_TYPE;

typedef enum EnumPolarityType
{
	ePT_LOW		= 0,
	ePT_HIGH	= 1
} ENUM_POLARITY_TYPE;

typedef enum EnumConnectState
{
	eCS_DISCONNECT		= 0,
	eCS_CONNECT			= 1,
	eCS_NEED_RECONNECT	= 2
} ENUM_CONNECT_STATE;

typedef enum EnumErrorType
{
	eET_ERR01	= 0,
	eET_ERR02,
	eET_ERR03,
	eET_ERR04,
	eET_ERR05,
	eET_ERR06,
	eET_ERR07,
	eET_ERR08,
	eET_ERR09,
	eET_ERR10,
	eET_ERR11,
	eET_ERR12,
	eET_ERR13,
	eET_ERR14,
	eET_ERR15,
	eET_ERR16,
	eET_ERR17,
	eET_ERR18,
	eET_ERR19,
	eET_ERR20,
	eET_ERR21,
	eET_ERR22,
	eET_ERR23,
	eET_ERR24,
	eET_ERR25
} ENUM_ERROR_TYPE;

typedef enum EnumExecTaskType
{
	eETT_KILL =0,
	eETT_STOP,
	eETT_CONT
} ENUM_EXECTASK_TYPE;

//---------------------------------------------------------------------------
// Class
//---------------------------------------------------------------------------
class CAxis;

//---------------------------------------------------------------------------
// Initialization
//---------------------------------------------------------------------------
CAxis* __stdcall MPI_CreateAxis( char *pszAxisName, ENUM_UNIT_TYPE enumUnitType, ENUM_DRIVE_TYPE enumDriveType );
void __stdcall MPI_DestroyAxis( CAxis *pAxis );
int __stdcall MPI_InitialAxis( CAxis *pAxis, double dVel, double dAcc, double dDec, double dDecKill,
	int nSmoothTime, double dInPosWidth, double dPosLimitPos, double dNegLimitPos );
void __stdcall MPI_GetVersion( char *pszVer );

//---------------------------------------------------------------------------
// Communication
//---------------------------------------------------------------------------
int __stdcall MPI_ConnectMegaulink( bool blNetWorkSetting=true );
int __stdcall MPI_Disconnect( );																// Disconnect the mega-ulink communication 
int __stdcall MPI_GetSlaveAmount( int *pnSlaveAmount );											// Get slave amount of the connected drives
void __stdcall MPI_GetComSuccess( int *pnComState );											// Get connect state of the mega-ulink communication

//---------------------------------------------------------------------------
// Access Variables
//---------------------------------------------------------------------------
int __stdcall MPI_GetSlaveID( CAxis *pAxis, int *pnSlaveID );									// Get the drive slave id
int __stdcall MPI_GetAxisName( CAxis *pAxis, char *pszAxisName );								// Get the drive axis name
int __stdcall MPI_GetFeedbackPos( CAxis *pAxis, double *pdPos );								// Get the drive axis feedback position
int __stdcall MPI_GetFeedbackVel( CAxis *pAxis, double *pdVel );								// Get the drive axis feedback velocity
int __stdcall MPI_GetVel( CAxis *pAxis, double *pdVel );										// Get the drive axis maximum velocity
int __stdcall MPI_GetAcc( CAxis *pAxis, double *pdAcc );										// Get the drive axis acceleration
int __stdcall MPI_GetDec( CAxis *pAxis, double *pdDec );										// Get the drive axis deceleration
int __stdcall MPI_GetDecKill( CAxis *pAxis, double *pdDecKill );								// Get the drive axis emergency stop deceleration
int __stdcall MPI_GetSmoothTime( CAxis *pAxis, int *pnSmoothTime );								// Get the drive axis smooth time
int __stdcall MPI_GetInPosWidth( CAxis *pAxis, double *pdInPosWidth );							// Get the drive axis In-position width
int __stdcall MPI_GetSwLimitPos( CAxis *pAxis, double *pdPosLimitPos, double *pdNegLimitPos );	// Get the drive axis software limit position
int __stdcall MPI_SetVel( CAxis *pAxis, double dVel );											// Set the drive axis maximum velocity 
int __stdcall MPI_SetAcc( CAxis *pAxis, double dAcc );											// Set the drive axis acceleration
int __stdcall MPI_SetDec( CAxis *pAxis, double dDec );											// Set the drive axis deceleration
int __stdcall MPI_SetDecKill( CAxis *pAxis, double dDecKill );									// Set the drive axis emergency stop deceleration
int __stdcall MPI_SetSmoothTime( CAxis *pAxis, int nSmoothTime );								// Set the drive axis smooth time
int __stdcall MPI_SetInPosWidth( CAxis *pAxis, double dInPosWidth );							// Set the drive axis In-position width
int __stdcall MPI_SetSwLimitPos( CAxis *pAxis, double dPosLimitPos, double dNegLimitPos );		// Set the drive axis software limit position

//---------------------------------------------------------------------------
// Axis Motion Function
//---------------------------------------------------------------------------
int __stdcall MPI_ServoOn( CAxis *pAxis );														// Enable axis
int __stdcall MPI_ServoOff( CAxis *pAxis );														// Disable axis
int __stdcall MPI_MoveAbsolute( CAxis *pAxis, double dPos, 
	double dVel=0, double dAcc=0, double dDec=0 );												// Absoulte motion
int __stdcall MPI_MoveRelative( CAxis *pAxis, double dPos, 
	double dVel=0, double dAcc=0, double dDec=0 );												// Relative motion
int __stdcall MPI_JogPositive( CAxis *pAxis, double dVel );										// Positive Jog motion
int __stdcall MPI_JogNegative( CAxis *pAxis, double dVel );										// Negative Jog motion
int __stdcall MPI_StopMotion( CAxis *pAxis );													// Stop motion
int __stdcall MPI_StartHome( CAxis *pAxis );													// Axis homing
int __stdcall MPI_SetZero( CAxis *pAxis );														// Set zero position
int __stdcall MPI_ResetDrive( CAxis *pAxis );													// Reset the controller
int __stdcall MPI_ClearError( CAxis *pAxis );													// Clear drive error
int __stdcall MPI_EnableSwLimit( CAxis *pAxis );												// Enable the sw limit protection
int __stdcall MPI_DisableSwLimit( CAxis *pAxis );												// Disable the sw limit protection
int __stdcall MPI_GetErrorData( CAxis *pAxis, int *pnErrorData );                      			// Get axis error(s)

//---------------------------------------------------------------------------
// States Access Function
//---------------------------------------------------------------------------
int __stdcall MPI_IsReady( CAxis *pAxis, bool *pblReady );										// Check if axis is in READY
int __stdcall MPI_IsHomed( CAxis *pAxis, bool *pblHomed );										// Check if axis is in HOMED
int __stdcall MPI_IsStopped( CAxis *pAxis, bool *pblStop );										// Check if axis is in STOP
int __stdcall MPI_IsMoving( CAxis *pAxis, bool *pblMoving );									// Check if axis is in MOVING
int __stdcall MPI_IsError( CAxis *pAxis, bool *pblError );										// Check if axis is in ERROR
int __stdcall MPI_IsSwLimitEn( CAxis *pAxis, bool *pblSwLimitEn );								// Check if axis is in LIMIT_EN
int __stdcall MPI_IsHwLimit( CAxis *pAxis, bool *pblLeftLimit, bool *pblRightLimit );			// Check if axis is in HW_LL and HW_RL
int __stdcall MPI_IsSwLimit( CAxis *pAxis, bool *pblLeftLimit, bool *pblRightLimit );			// Check if axis is in SW_LL and SW_RL
int __stdcall MPI_WaitServoReady( CAxis *pAxis, unsigned int nTimeOut );						// Waiting the servo ready
int __stdcall MPI_WaitHomeOver( CAxis *pAxis, unsigned int nTimeOut );							// Waiting the homing process over
int __stdcall MPI_WaitMoveOver( CAxis *pAxis, unsigned int nTimeOut );							// Waiting the moving process over
int __stdcall MPI_IsTaskRunning( CAxis *pAxis, int nTaskID, bool *plRunning );					// Check if pdl task is RUNNING

//---------------------------------------------------------------------------
// General Input/Output Function
//---------------------------------------------------------------------------
int __stdcall MPI_GetInputStatus( CAxis *pAxis, int nInputIndex, bool *pblStatus );				// Get the input status
int __stdcall MPI_GetOutputStatus( CAxis *pAxis, int nOutputIndex, bool *pblStatus );			// Get the output status
int __stdcall MPI_SetOutput( CAxis *pAxis, int nOutputIndex );									// Set on the output
int __stdcall MPI_ClearOutput( CAxis *pAxis, int nOutputIndex );								// Set off the output
int __stdcall MPI_ToggleOutput( CAxis *pAxis, int nOutputIndex );								// Toggle the output

//---------------------------------------------------------------------------
// Specific Input/Output Function
//---------------------------------------------------------------------------
int __stdcall MPI_SetTriggerPosition( CAxis *pAxis, double dStartPos, double dEndPos, 
	double dInterval=0 );																		// Set position trigger position
int __stdcall MPI_SetTriggerInterval( CAxis *pAxis, double dInterval );							// Set position trigger interval
int __stdcall MPI_EnableTrigger( CAxis *pAxis, ENUM_POLARITY_TYPE enumPolarity=ePT_LOW, 
	int nPulseWidth=0 );																		// Enable position trigger function
int __stdcall MPI_DisableTrigger( CAxis *pAxis );												// Disable position trigger function

//---------------------------------------------------------------------------
// Access Specific Parameter Function
//---------------------------------------------------------------------------
int __stdcall MPI_SetVar( CAxis *pAxis, char *pszVarName, double dValue );						// Set specfic variable
int __stdcall MPI_SetState( CAxis *pAxis, char *pszStateName, bool blStatus );					// Set specfic state
int __stdcall MPI_GetVar( CAxis *pAxis, char *pszVarName, double *pdValue );					// Get specfic variable
int __stdcall MPI_GetState( CAxis *pAxis, char *pszStateName, bool *pblStatus );				// Get specfic state
int __stdcall MPI_RunFuncPDL( CAxis *pAxis, char *pszPDLName, int *pnTaskID );					// Run specfic pdl function
int __stdcall MPI_KillTask( CAxis *pAxis, int nTaskID, ENUM_EXECTASK_TYPE  enumExecTaskType = eETT_KILL );  //Kill/stop/continue task
