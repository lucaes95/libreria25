package com.unina.libreria25.model;

public class Messaggio {
    private int codMsg;
    private int codPrestito;
    private String testo;
    private String data;

    public Messaggio(int codMsg, int codPrestito, String testo, String data) {
        this.codMsg = codMsg;
        this.codPrestito = codPrestito;
        this.testo = testo;
        this.data = data;
    }

    public int getCodMsg() {
        return codMsg;
    }

    public int getCodPrestito() {
        return codPrestito;
    }

    public String getTesto() {
        return testo;
    }

    public String getData() {
        return data;
    }
}
