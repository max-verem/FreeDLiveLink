#ifndef FreeD_h
#define FreeD_h

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define FREE_D_D1_PACKET_SIZE 29

typedef struct
{
    unsigned int ID;        // Camera ID

    double Pan;             // Camera Pan Angle
    double Tilt;            // Camera Tilt Angle
    double Roll;            // Camera Roll Angle
    double X;               // Camera X - Position
    double Y;               // Camera Y - Position
    double Z;               // Camera Height(Z - Position)
    int Zoom;               // Camera Zoom
    int Focus;              // Camera Focus
    unsigned char Spare[2]; // Spare / User Defined(16 bits)

} FreeD_D1_t;

int FreeD_D1_unpack(unsigned char *buf, int len, FreeD_D1_t* dst);
int FreeD_D1_pack(unsigned char *buf, int len, FreeD_D1_t* src);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* FreeD_h */