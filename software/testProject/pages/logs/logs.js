//logs.js
const util = require('../../utils/util.js')
import { Config } from '../../utils/config.js';

Page({
  data: {
    logs: [],
    img:''
  },
  onLoad: function () {
    this.getLogImg();
    this.setData({
      logs: (wx.getStorageSync('logs') || []).map(log => {
        return util.formatTime(new Date(log))
      })
    })
  },
  transenter(){
    wx.navigateTo({
      url: '../tran_login/tran_login'
    })
  },//../trans_order/trans_order
  consumerenter: function(){
    wx.navigateTo({
      url: '../user_login/user_login'
    })
  },//../user_menu/user_menu
  getLogImg:function(){
    var that = this;
    wx.request({
      url: Config.restUrl + '/api/v1/mix/getimg/1',
      data: '',
      header: {
        'content-type': 'application/json' // 默认值
      },
      method: 'get',
      success(res) {
        //console.log(res.data)
        var dataxx = Config.restUrl+'/img'+res.data;
        //console.log("赋值完成")
        that.setData2page(dataxx)
      }
    })
  },
  setData2page: function (data) {
    //console.log("我正在工作");
    this.setData({
      img: data
    });
    //console.log(this.data.img);
  },
})
