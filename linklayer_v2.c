#include "linklayer.h"

//Constantes definidas usadas no protocolo
#define FLAG 0x5c
#define SET 0x03
#define UA 0x07
#define DISC 0x0B
#define RR0 0x01
#define RR1 0x21
#define REJ0 0x05
#define REJ1 0x25
#define A0 0x01
#define A1 0x03
#define I0 0x00
#define I1 0x02
#define ESCAPE_FLAG 0X5d
#define STUFF_BYTE 0x20

struct termios oldtio,newtio;
int fd, tempo_esgotado, stop;
unsigned char Set[5]
unsigned char Ua[5];
unsigned char DISCR[5];
unsigned char DISCT[5];
unsigned char AUX_Frame[5];
    
typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} States;


//Alarmes
void expira() {
    tempo_esgotado = 1;//tempo limite esgotado
}

void inicia_alarme(int tempo) { 
    tempo_esgotado=0; //ainda não aingimos o tempo limite
    signal(SIGALRM, expira); //Quando o tempo esgotar o sistema envia um SIGALRM e por consequencia é executado a funcao expira 
    alarm(tempo)//Configura o temporizador para os segundos dado na variavel tempo e provoca um SIGLRM após esse time
}
//Função para enviar trama
    int frame_transmition(int fd, unsigned char* frame, int size){
        
        int transmition = write(fd, frame, size);
        if(transmition == -1){
             perror("write");
        return -1;
        }
        return transmition; 
    }
// MAQUINA DE ESTADOS
    int frame_confirmation(int fd, unsigned char* frame){//Recebe uma trama e confirma se é válida
    
        int received, i = 0;
        States current_state = START;
        unsigned char aux;

        while(current_state != STOP){
            received = read(fd, &aux, 1);
            if(received == -1){
                return -1
            }
            
            switch(current_state) {
            case START:
                
                if(aux == FLAG) {  
                    frame[i++] = aux;
                    current_state = FLAG_RCV;
                }
                break;

            case FLAG_RCV:
                
                if(aux == A0 || aux == A1) { 
                    frame[i++] = aux;
                    current_state = A_RCV;
                } 
                
                else if(aux != FLAG) {  
                    i = 0;
                    current_state = START;
                }
                break;

            case A_RCV:
                
                if(aux == SET || aux == DISC || aux == UA || aux == RR0 || aux == RR1 || aux == REJ0 || aux == REJ1 || aux == I0 || aux == I1) {
                    frame[i++] = aux;
                    current_state = C_RCV;
                } 
                
                else if(aux == FLAG) {
                    i = 1;
                    current_state = FLAG_RCV;
                } 
                
                else {
                    i = 0;
                    current_state = START;
                }
                break;

            case C_RCV:
                
                if(aux == (frame[1] ^ frame[2])) {
                    frame[i++] = aux;
                    current_state = BCC_OK;
                } 
               
                else if(aux == FLAG) {
                    i = 1;
                    current_state = FLAG_RCV;
                } 
                
                else {
                    current_state = START;
                }
                break;
            
            case BCC_OK:
           
                if(aux == FLAG) {
                    frame[i++] = aux;
                    current_state = STOP;
                } 
                
                else if(i < 2 * MAX_PAYLOAD_SIZE + 5) {
                    frame[i++] = aux;
                }
                
                else {
                    current_state = START;
                }
                break;

            case STOP:
                break;

            default:
                break;
            }
        }

    return i;

            
}


//LLOPEN
int llopen(linkLayer connectionParameters){
    //Inicializa tramas
    Set[0] = FLAG;
    Set[1] = A0;
    Set[2] = SET;
    Set[3] = A0 ^ SET;
    Set[4] = FLAG;
    Ua[0] = FLAG;
    Ua[1] = A0;
    Ua[2] = UA;
    Ua[3] = A0 ^ UA;
    Ua[4] = FLAG;

    int confirmation, transmition, counter = 0;

    fd = open(link_layer.serialPort, O_RDWR | O_NOCTTY);
       
     if (fd_r < 0){
        perror(link_layer.port);
        exit(-1);
    }

    if (tcgetattr(fd,&oldtio) == -1) { /* Guarda em Oldtio os parametros antigas da porta especificada em fd*/
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio)); //Inicializa newtio com 0s
    newtio.c_cflag = link_layer.baudRate | CS8 | CLOCAL | CREAD; //Guarda no campo cflag do newtio as configurações provenientes da combinaçao da baudrate (taxa de transfer) e os restantes perametros (Cs8 - tamanho do caracter é 8 bits/ Clocal-Estamos a usar a porta localmente e não externamente/ Cread - A porta está destinada a receber dados)
    newtio.c_iflag = IGNPAR;  //Ignora os erros de paridade
    newtio.c_oflag = 0; //Isto estabelece que no output a porta não irá ter configurações especias sendo tratada como default e não sofrendo alterações
    newtio.c_lflag = 0; //Isto estabelece que localmente a porta não irá ter configurações especias sendo tratada como default e não sofrendo alterações, e apenas efetua o necessário para receber e enviar os dados sem alteração
    newtio.c_cc[VTIME]    = 0;   //Desativa o tempo de espera entre caracteres, assim que ler um caracter está pronto para o proximo sem ter de esperar
    newtio.c_cc[VMIN]     = 1;   //Aguarda até ser recebido um caracter e não retorna até tal acontecer

    if(tcflush(fd, TCIOFLUSH) == -1){
        perror("tcflush");
        exit(-1);
    }  

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) { //Guarda em fd as novas configurações definidas na variavel newtio
        perror("tcsetattr");
        exit(-1);
    }

    if(connectionParameters.role == 0){ //Para o caso do recetor
        transmition = frame_transmition(fd, Set, 5); //Recetor emite um set
        if(transmition < 0){//Verifica se está tudo bem
            return -1;
        }
        

        inicia_alarme(connectionParameters.timeOut);
        while(1){ //Espera até receber Ua ou até esgotar o tempo
            confirmation = frame_confirmation(fd, AUX_Frame);

            if(tempo_esgotado == 1){ //Quando o tempo esgota
                if(counter < connectionParameters.numTries){
                    transmition = frame_transmition(fd,Set , 5);
                    if(transmition < 0){
                        return -1;
                    }
                    inicia_alarme(connectionParameters.timeOut);
                    counter++;
                }
                else{
                    return -1;
                }
            }
             //Recebe Ua antes de esgotar o tempo
            if(confirmation > 0 && Ua[0] == AUX_Frame[0] && Ua[1] == AUX_Frame[1] && Ua[2] == AUX_Frame[2] && Ua[3] == AUX_Frame[3] && Ua[4] == AUX_Frame[4]){
                
                inicia_alarme(0);
                break;//Recebeu o Ua então ligação estabelecida
            }
        }
    }

    if(connectionParameters.role == 1){ //Para o caso do recetor
        
        while(1) { //Espera até receber set
           
           confirmation = frame_confirmation(fd, Set);
           if(confirmation < 0){
                return -1; //verifica se foi recebido com sucesso
            }
           
           if(confirmation > 0 && Set[0] == AUX_Frame[0] && Set[1] == AUX_Frame[1] && Set[2] == AUX_Frame[2] && Set[3] == AUX_Frame[3] && Set[4] == AUX_Frame[4]) {
                transmition=frame_transmition(fd, Ua, 5); //Quando finalmente recebe um set envia um Ua de resposta
                if(transmition < 0) {
                    return -1;
                }
                break; //Finaliza a espera
           }
        }    
    }

    return 0;

}   

//LLWRITE
int llwrite(char* buf, int bufSize){
    unsigned char frame[2 * MAX_PAYLOAD_SIZE + 6], BCC2 = 0;
    int confirmation, transmition, i, f;

    for(i = 0; i + 1 < bufSize; i++){
        BCC2 ^= buf[i];
    }

    frame[f++] = FLAG;
    frame[f++] = A0;
    frame[f++] = I0;
    frame[f++] = A0 ^ I0;

    for(i = 0; i < bufSize; i++){
        if(buf[i] == FLAG || buf[i] == ESCAPE_FLAG){
            frame[f++] = ESCAPE_FLAG;
            frame[f++] = BCC2 ^ STUFF_BYTE;
        }
        else{
            frame[f++] = buf[i];
        }
    }

    if(BCC2 == FLAG || BCCC2 == ESCAPE_FLAG){
        frame[f++] = ESCAPE_FLAG;
        frame[f++] = BCC2 ^ STUFF_BYTE;
    }
    else{
        frame[f++] = BCC2;
    }

    frame[f++] = FLAG;
    
    transmition = frame_transmition(fd, frame, f); //começa por mandar uma trama inicial
    if(transmition < 0) return -1;
    
    while(1) { //Repete até finalizar a trasferencia em que recebe o RR1

        confirmation = frame_confirmation(fd, AUX_Frame); //Tenta receber uma trama
        if(confirmation < 0) return -1; //se não estiver bem retorna -1
        
        //Recebe a trama RR0
        if(AUX_Frame[0] == F && AUX_Frame[1] == A0 && AUX_Frame[2] == C_RR0 && AUX_Frame[3] == (A0 ^ C_RR0) && AUX_Frame[4] == F && confirmation>0) {
            frame[2] = C_I1; //substitui na frame os bytes necessarios para enviar a segunda trama I
            frame[3] = A0 ^ C_I1;
            transmition = frame_transmition(fd, frame, f); //envia a trama I
            if(transmition < 0) return -1;
        } 
        //Recebe a trama RR1
        else if(AUX_Frame[0] == F && AUX_Frame[1] == A0 && AUX_Frame[2] == C_RR1 && AUX_Frame[3] == (A0 ^ C_RR1) && AUX_Frame[4] == F && confirmation > 0) {
            break; //Quando recebe RR1 finaliza
        } 
        //Recebe a trama REJ0
        else if(AUX_Frame[0] == F && AUX_Frame[1] == A0 && AUX_Frame[2] == C_REJ0 && AUX_Frame[3] == (A0 ^ C_REJ0) && AUX_Frame[4] == F && confirmation > 0) {
            transmition = frame_transmition(fd, frame, f); //Quando recebe a trama REJ0 reenvia a 1ª trama I pois nao recebeu corretamente
            if(transmition < 0) return -1;
        }
        //Recebe a trama REJ1
        else if(receive > 0 && AUX_Frame[0] == F && AUX_Frame[1] == A0 && AUX_Frame[2] == C_REJ1 && AUX_Frame[3] == (A0 ^ C_REJ1) && AUX_Frame[4] == F && confirmation > 0) {
            transmition = frame_transmition(fd, frame, f); //Quando recebe a trama REJ1 reenvia a 2ª trama I pois nao recebeu corretamente
            if(transmition < 0) return -1;
        }
    }

    return f;
}


//LLREAD
int llread(char* packet) {
    unsigned char frame[2 * MAX_PAYLOAD_SIZE + 6], BCC2 = 0, Rej0[5], Rej1[5], Rr0[5], Rr1[5];
    int confirmation, transmition, i, f;
    Rej0[0] = FLAG;
    Rej0[1] = A0;
    Rej0[2] = REJ0;
    Rej0[3] = A0 ^ REJ0;
    Rej0[4] = FLAG;
    
    Rej1[0] = FLAG;
    Rej1[1] = A0;
    Rej1[2] = REJ1;
    Rej1[3] = A0 ^ REJ1;
    Rej1[4] = FLAG;

    Rr0[0] = FLAG;
    Rr0[1] = A0;
    Rr0[2] = RR0;
    Rr0[3] = A0 ^ RR0;
    Rr0[4] = FLAG;

    Rr1[0] = FLAG;
    Rr1[1] = A0;
    Rr1[2] = RR1;
    Rr1[3] = A0 ^ RR1;
    Rr1[4] = FLAG;
    

    while(1){ //Repete até conseguir enviar com sucesso a trama RR1
        confirmation = frame_confirmation(fd, frame); //Espera receber uma trama
        if(confirmation < 0){ 
        return -1;
        }
         if(confirmation > 0 && frame[0] == FLAG && frame[1] == A0 && frame[2] == I0 && frame[3] == (A0 ^ I0) && frame[confirmation - 1] == FLAG){
            for(i = 4; i < confirmation -2; i++){
                if(frame[i] == ESCAPE_FLAG){
                    if(i == confirmation -3){
                        frame[confirmation - 2] ^= STUFF_BYTE;
                    }
                    else{
                        packet[f++] = frame[++i] ^ STUFF_BYTE;

                    }
                }
                else{
                    packet[f++] = frame[i];
                }
            }
            
            for(i = 0; i + 1 < f; i++){
                BCC2 ^= packet[i];
            }

            if(BCC2 != frame[confirmation - 2]){
                transmition = frame_transmition(fd, Rej0, 5);
                if(transmition < 0){
                    return -1;
                }
            }
            else{
                transmition = frame_transmition(fd, Rr0, 5);
                if(transmition < 0){
                    return -1;
                }
            } 
        }

        if(confirmation > 0 && frame[0] == FLAG && frame[1] == A0 && frame[2] == I1 && frame[3] == (A0 ^ I1) && frame[confirmation - 1] == FLAG){
            if(frame[confirmation - 3] == ESCAPE_FLAG) {
                frame[confirmation - 2] ^= STUFF_BYTE;
            }
            if(BCC2 != frame[confirmation - 2]){
                transmition = frame_transmition(fd, Rej1, 5);
                if(transmition < 0){
                    return -1;
                }
            }
            else{
                 transmition = frame_transmition(fd, Rr1, 5);
                if(transmition < 0){
                    return -1;
                }
                break;
            }
        }  
    }

    return f;
}
//LLCLOSE
int llclose(linkLayer connectionParameters, int showStatistics){
     //Inicializa tramas
    Ua[0] = FLAG;
    Ua[1] = A1;
    Ua[2] = UA;
    Ua[3] = A1 ^ UA;
    Ua[4] = FLAG;
    
    DISCT[0] = FLAG;
    DISCT[1] = A0;
    DISCT[2] = DISC;
    DISCT[3] = A0 ^ DISC;
    DISCT[4] = FLAG;
    
    DISCR[0] = FLAG;
    DISCR[1] = A1;
    DISCR[2] = DISC;
    DISCR[3] = A1 ^ DISC;
    DISCR[4] = FLAG;

    //variaveis auxiliares
    int transmition, confirmation;


    if(connectionParameters.role == 0) { //Para o caso do emissor
       
       transmition = frame_transmition(fd, DISCT, 5); //manda um disc
        if(transmition < 0) {//verifica se não ocorre problema
            return -1;
        }
        
        while(1) { //Espera até receber um disc de volta

            confirmation = frame_confirmation(fd, AUX_Frame);
            if(confirmation < 0) {//verifica se não ocorre problema
                    return -1;
                }
                
            if((DISCR[0] == AUX_Frame[0]) && (DISCR[1] == AUX_Frame[1]) && (DISCR[2] == AUX_Frame[2]) && (DISCR[3] == AUX_Frame[3]) && (DISCR[4] == AUX_Frame[4]) && (confirmation > 0)) {
                transmition = frame_transmition(fd, Ua, 5); //quando finalmente recebe um disc emite um Ua 
                break;
            }
        }
    }

    if(connectionParameters.role == 1) { //Para o caso do recetor

        while(1) { //Espera até receber um disc vindo do emissor

            confirmation = frame_confirmation(fd, AUX_Frame);
            if(confirmation < 0) { //verifica se não ocorre problema
                    return -1;
            }
                
            if((DISCT[0] == AUX_Frame[0]) && (DISCT[1] == AUX_Frame[1]) && (DISCT[2] == AUX_Frame[2]) && (DISCT[3] == AUX_Frame[3]) && (DISCT[4] == AUX_Frame[4]) && (confirmation > 0)) {
                transmition = frame_transmition(fd, DISCR, 5); //quando finalmente recebe um disc emite um disc de volta
                if(transmition < 0) { //verifica se não ocorre problema
                    return -1;
                }
            }

            if((Ua[0] == AUX_Frame[0]) && (Ua[1] == AUX_Frame[1]) && (Ua[2] == AUX_Frame[2]) && (Ua[3] == AUX_Frame[3]) && (Ua[4] == AUX_Frame[4]) && (confirmation > 0)) {
                break; //quando recebe o Ua proveniente do emissor pode finalmente fechar
            }

        }
    }

     if(tcflush(fd, TCIOFLUSH) == -1) { //Da clear na ligacao
        perror("tcflush");
        return -1;
    }

    
    if(tcsetattr(fd, TCSANOW, &oldtio) == -1) { //Retoma as especificações da ligação para as antigas
        perror("tcsetattr");
        return -1;
    }

    
    if(close(fd) == -1) { //Fecha a ligação
        perror("close");
        return -1;
    }

    return 0;

}



