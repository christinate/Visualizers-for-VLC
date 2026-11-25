'use strict';
const {contextBridge,ipcRenderer}=require('electron');
const invoke=(channel,...args)=>ipcRenderer.invoke(channel,...args);
contextBridge.exposeInMainWorld('visualizer',{
  config:()=>invoke('config'),
  audio:()=>invoke('audio-read'),
  writeAudio:(samples,rate)=>invoke('audio-write',samples,rate),
  chooseAudio:()=>invoke('audio-choose'),
  captureReady:()=>invoke('capture-ready'),
  captureSelected:(device,label)=>invoke('capture-selected',device,label),
  captureCancel:()=>invoke('capture-cancel'),
  fullscreen:()=>invoke('fullscreen'),
  close:()=>invoke('close'),
  settings:()=>invoke('settings'),
  saveSettings:values=>invoke('settings-save',values),
  report:value=>invoke('report',value),
  onCaptureSelection:handler=>ipcRenderer.on('capture-select',(_event,value)=>handler(value)),
  onSourceChanged:handler=>ipcRenderer.on('audio-source-changed',(_event,value)=>handler(value))
});
