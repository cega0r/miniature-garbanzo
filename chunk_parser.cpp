#include "chunk_parser.h"

int ChunkParser::ReadVarInt(const std::vector<uint8_t>& b, size_t& p) {
    int r=0,s=0;
    while(p<b.size()){uint8_t byte=b[p++];r|=(byte&0x7F)<<s;if(!(byte&0x80))break;s+=7;}
    return r;
}

void ChunkParser::DecodeSection(const std::vector<uint8_t>& b, size_t& p,
                                 int cx, int cz, int sy, OreFoundCallback& cb) {
    p+=2; uint8_t bpe=b[p++];
    std::vector<int> palette;
    if(bpe==0){
        int id=ReadVarInt(b,p); ReadVarInt(b,p);
        if(ORE_IDS.count(id))
            for(int y=0;y<16;y++)for(int z=0;z<16;z++)for(int x=0;x<16;x++)
                cb({cx,cz,x,sy*16+y,z,ORE_IDS.at(id)});
        return;
    }
    if(bpe<=8){int pl=ReadVarInt(b,p);for(int i=0;i<pl;i++)palette.push_back(ReadVarInt(b,p));}
    int dl=ReadVarInt(b,p);
    std::vector<uint64_t> data(dl);
    for(int i=0;i<dl;i++){uint64_t v=0;for(int j=7;j>=0;j--)v=(v<<8)|b[p++];data[i]=v;}
    int vpl=64/bpe; uint64_t mask=(1ULL<<bpe)-1;
    for(int i=0;i<4096;i++){
        int li=i/vpl,bi=(i%vpl)*bpe;
        if(li>=(int)data.size())break;
        int si=(int)((data[li]>>bi)&mask);
        int id=(bpe<=8&&si<(int)palette.size())?palette[si]:si;
        if(ORE_IDS.count(id)){
            int lx=i&0xF,ly=(i>>8)&0xF,lz=(i>>4)&0xF;
            cb({cx,cz,lx,sy*16+ly,lz,ORE_IDS.at(id)});
        }
    }
}

void ChunkParser::Parse(const std::vector<uint8_t>& pkt, OreFoundCallback cb, int minY) {
    size_t p=0; ReadVarInt(pkt,p);
    if(p+8>pkt.size())return;
    int cx=(int)((pkt[p]<<24)|(pkt[p+1]<<16)|(pkt[p+2]<<8)|pkt[p+3]); p+=4;
    int cz=(int)((pkt[p]<<24)|(pkt[p+1]<<16)|(pkt[p+2]<<8)|pkt[p+3]); p+=4;
    if(p<pkt.size()&&pkt[p]==0x0A){
        p++; p+=2; int depth=1;
        while(p<pkt.size()&&depth>0){
            uint8_t tag=pkt[p++];
            if(tag==0x00){depth--;continue;}
            if(tag==0x0A){depth++;p+=2+((pkt[p]<<8)|pkt[p+1]);continue;}
            uint16_t nl=(pkt[p]<<8)|pkt[p+1]; p+=2+nl;
            switch(tag){
                case 1:p+=1;break;case 2:p+=2;break;
                case 3:case 5:p+=4;break;case 4:case 6:p+=8;break;
                case 7:{int l=((pkt[p]<<24)|(pkt[p+1]<<16)|(pkt[p+2]<<8)|pkt[p+3]);p+=4+l;break;}
                case 8:{uint16_t l=(pkt[p]<<8)|pkt[p+1];p+=2+l;break;}
                case 11:{int l=((pkt[p]<<24)|(pkt[p+1]<<16)|(pkt[p+2]<<8)|pkt[p+3]);p+=4+l*4;break;}
                case 12:{int l=((pkt[p]<<24)|(pkt[p+1]<<16)|(pkt[p+2]<<8)|pkt[p+3]);p+=4+l*8;break;}
            }
        }
    }
    ReadVarInt(pkt,p);
    int ns=24, ss=minY/16;
    for(int s=0;s<ns&&p<pkt.size();s++){
        int wsy=ss+s;
        DecodeSection(pkt,p,cx,cz,wsy,cb);
        uint8_t bpe=pkt[p++];
        if(bpe==0){ReadVarInt(pkt,p);ReadVarInt(pkt,p);}
        else{int pl=ReadVarInt(pkt,p);for(int i=0;i<pl;i++)ReadVarInt(pkt,p);int dl=ReadVarInt(pkt,p);p+=dl*8;}
    }
}
