#ifndef REDIRECT_TO_PORTAL_H
#define REDIRECT_TO_PORTAL_H

const char REDIRECT_TO_PORTAL_TEMPLATE[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8">
        <meta http-equiv="X-UA-Compatible" content="IE=edge">
        <title>%THE_TEMPLATED_TITLE%</title>
        <meta name="description" content="ClusterDuck Network Redirect by the ClusterDuck Protocol">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            body {
                font: 14px "Avenir", helvetica, sans-serif;
                -webkit-font-smoothing: antialiased;
            }
            h2 {
                text-align: center;
                color: #111;
                margin: 16px 0 8px;
            }
            h3 {
                font-size: 14px;
                margin: 0 0 24px;
                color: #111;
                font-weight: 400;
            }
            h6 {
                font-size: 12px;
                font-weight: 300;
                line-height: 16px;
                margin: 16px 0 0;
                color: rgba(0,0,0,.5);
            }
            p {
                color: #111;
            }
            .content {
                text-align: center;
                padding: 0 16px;
            }
            .body.on {
               display: block;
            }
            .body.off {
               display: none;
            }
            .body.sent {
            }
            .body.sent .c {
               background: #fff;
               color: #111;
               width: auto;
               max-width: 80%;
               margin: 0 auto;
               padding: 14px;
            }
            .body.sent .c h4 {
               margin: 0 0 1em;
               font-size: 1.5em;
            }
            .b {
               display: block;
               padding: 20px;
               text-align: center;
               cursor: pointer;
            }
            .b:hover {
               opacity: .7;
            }
            .update {
               border: 0;
               background: #fe5454;
            }
            .redirectButton {
                box-shadow: 0px 1px 0px 0px #fff6af;
                background:linear-gradient(to bottom, #ffec64 5%, #ffab23 100%);
                background-color:#ffec64;
                border-radius:6px;
                border:1px solid #ffaa22;
                display:block;
                cursor:pointer;
                color:#333333;
                font-family:Arial;
                font-size:15px;
                font-weight:bold;
                padding: 10px 24px;
                text-decoration: none;
                text-shadow: 0px 1px 0px #ffee66;
                text-align: center;
                width: 100%;
                margin-top: 10px;
            }
            .redirectButton:hover {
                background:linear-gradient(to bottom, #ffab23 5%, #ffec64 100%);
                background-color:#ffab23;
            }
            .redirectButton:active {
                position:relative;
                top:1px;
            }
        </style>
        </head>
    <body>
        <h2 class="">You are Connected to a ClusterDuck</h2>
        <div class="content body" id="formContent">
            <h3><a href="http://192.168.1.1/portal">Go to portal</a></h3>
        </div>
        <h2>%THE_TEMPLATED_TITLE%</h2>
    </body>
    </html>
)=====";

#endif
